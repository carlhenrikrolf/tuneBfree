/**
 * CLAP for tuneBfree
 *
 * Based on nakst's CLAP tutorial at:
 *
 *  https://nakst.gitlab.io/tutorial/clap-part-2.html
 */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <math.h>
#include <string>

#include "clap/clap.h"
#include "readerwriterqueue.h"

#include "tonegen.h"
#include "overdrive.h"
#include "reverb.h"
#include "whirl.h"

#define MIN(A, B) (((A) < (B)) ? (A) : (B))

// Parameters.
#define P_DRAWBAR_MIN (0)
#define P_DRAWBAR_MAX (8)
#define P_VIBRATO (9)
#define P_VIBRATO_TYPE (10)
#define P_DRUM (11)
#define P_HORN (12)
#define P_OVERDRIVE (13)
#define P_CHARACTER (14)
#define P_REVERB (15)
#define P_PERCUSSION (16)
#define P_PERCUSSION_VOLUME (17)
#define P_PERCUSSION_DECAY (18)
#define P_PERCUSSION_HARMONIC (19)
#define P_COUNT (20)

struct ParamMsg
{
    uint32_t paramIndex;
    double value;
};

static moodycamel::ReaderWriterQueue<ParamMsg, 4096> toAudioQ(128);
static moodycamel::ReaderWriterQueue<ParamMsg, 4096> fromAudioQ(128);

struct MyPlugin
{
    clap_plugin_t plugin;
    const clap_host_t *host;
    float sampleRate;
    float parameters[P_COUNT], mainParameters[P_COUNT];
    struct b_tonegen *synth;
    struct b_preamp *preamp;
    struct b_reverb *reverb;
    struct b_whirl *whirl;
    int boffset;
    float bufA[BUFFER_SIZE_SAMPLES];
    float bufB[BUFFER_SIZE_SAMPLES];
    float bufC[BUFFER_SIZE_SAMPLES];
    float bufD[2][BUFFER_SIZE_SAMPLES]; // drum, tmp.
    float bufL[2][BUFFER_SIZE_SAMPLES]; // leslie, out
};

void setParam(MyPlugin *plugin, uint32_t index, float value)
{
    if ((P_DRAWBAR_MIN <= index) && (index <= P_DRAWBAR_MAX))
    {
        setDrawBar(plugin->synth, index, rint(value));
    }
    else if (index == P_VIBRATO)
    {
        setVibratoUpper(plugin->synth, rint(value));
    }
    else if (index == P_VIBRATO_TYPE)
    {
        setVibratoFromInt(plugin->synth, floor(value));
    }
    else if ((index == P_DRUM) || (index == P_HORN))
    {
        useRevOption(
            plugin->whirl,
            (int)(floor(plugin->parameters[P_DRUM]) + 3 * floor(plugin->parameters[P_HORN])), 2);
    }
    else if (index == P_OVERDRIVE)
    {
        plugin->preamp->isClean = rint(1.0f - value);
    }
    else if (index == P_CHARACTER)
    {
        fsetCharacter(plugin->preamp, value);
    }
    else if (index == P_REVERB)
    {
        setReverbMix(plugin->reverb, value);
    }
    else if (index == P_PERCUSSION)
    {
        setPercussionEnabled(plugin->synth, rint(value));
    }
    else if (index == P_PERCUSSION_VOLUME)
    {
        setPercussionVolume(plugin->synth, 1 - rint(value));
    }
    else if (index == P_PERCUSSION_DECAY)
    {
        setPercussionFast(plugin->synth, rint(value));
    }
    else if (index == P_PERCUSSION_HARMONIC)
    {
        setPercussionFirst(plugin->synth, rint(value));
    }
}

static void PluginProcessEvent(MyPlugin *plugin, const clap_event_header_t *event)
{
    if (event->space_id == CLAP_CORE_EVENT_SPACE_ID)
    {
        if (event->type == CLAP_EVENT_NOTE_ON || event->type == CLAP_EVENT_NOTE_OFF ||
            event->type == CLAP_EVENT_NOTE_CHOKE)
        {
            const clap_event_note_t *noteEvent = (const clap_event_note_t *)event;

            if (event->type == CLAP_EVENT_NOTE_ON)
            {
                oscKeyOn(plugin->synth, noteEvent->key, noteEvent->key);
            }
            else
            {
                oscKeyOff(plugin->synth, noteEvent->key, noteEvent->key);
            }
        }
        else if (event->type == CLAP_EVENT_PARAM_VALUE)
        {
            const clap_event_param_value_t *valueEvent = (const clap_event_param_value_t *)event;
            uint32_t index = (uint32_t)valueEvent->param_id;

            plugin->parameters[index] = valueEvent->value;
            struct ParamMsg p = {index, valueEvent->value};
            fromAudioQ.try_enqueue(p);
#ifdef DEBUG_PRINT
            fprintf(stderr, "PluginProcessEvent fromAudioQ.try_enqueue: %d %f\n", p.paramIndex,
                    p.value);
#endif
            setParam(plugin, index, valueEvent->value);
        }
    }
}

static uint32_t synthSound(struct MyPlugin *plugin, uint32_t written, uint32_t nframes,
                           float *outputL, float *outputR)
{
    while (written < nframes)
    {
        int nremain = nframes - written;

        if (plugin->boffset >= BUFFER_SIZE_SAMPLES)
        {
            plugin->boffset = 0;
            oscGenerateFragment(plugin->synth, plugin->bufA, BUFFER_SIZE_SAMPLES);
            preamp(plugin->preamp, plugin->bufA, plugin->bufB, BUFFER_SIZE_SAMPLES);
            plugin->reverb->reverb(plugin->bufB, plugin->bufC, BUFFER_SIZE_SAMPLES);
            whirlProc3(plugin->whirl, plugin->bufC, plugin->bufL[0], plugin->bufL[1],
                       plugin->bufD[0], plugin->bufD[1], BUFFER_SIZE_SAMPLES);
        }

        int nread = MIN(nremain, (BUFFER_SIZE_SAMPLES - plugin->boffset));

        memcpy(&outputL[written], &plugin->bufL[0][plugin->boffset], nread * sizeof(float));
        memcpy(&outputR[written], &plugin->bufL[1][plugin->boffset], nread * sizeof(float));

        written += nread;
        plugin->boffset += nread;
    }
    return written;
}

static void PluginRenderAudio(MyPlugin *plugin, uint32_t start, uint32_t end, float *outputL,
                              float *outputR)
{
    synthSound(plugin, 0, end - start, outputL + start, outputR + start);
}

static void PluginSyncMainToAudio(MyPlugin *plugin, const clap_output_events_t *out)
{
    struct ParamMsg p;
    while (toAudioQ.try_dequeue(p))
    {
#ifdef DEBUG_PRINT
        fprintf(stderr, "PluginSyncMainToAudio toAudioQ.try_dequeue: %d %f\n", p.paramIndex,
                p.value);
#endif
        uint32_t i = p.paramIndex;
        plugin->parameters[i] = p.value;
        setParam(plugin, p.paramIndex, p.value);

        clap_event_param_value_t event = {};
        event.header.size = sizeof(event);
        event.header.time = 0;
        event.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
        event.header.type = CLAP_EVENT_PARAM_VALUE;
        event.header.flags = 0;
        event.param_id = i;
        event.cookie = NULL;
        event.note_id = -1;
        event.port_index = -1;
        event.channel = -1;
        event.key = -1;
        event.value = plugin->parameters[i];
        out->try_push(out, &event.header);
    }
}

static bool PluginSyncAudioToMain(MyPlugin *plugin)
{
    bool anyChanged = false;
    struct ParamMsg p;
    while (fromAudioQ.try_dequeue(p))
    {
#ifdef DEBUG_PRINT
        fprintf(stderr, "PluginSyncAudioToMain fromAudioQ.try_dequeue: %d %f\n", p.paramIndex,
                p.value);
#endif
        plugin->mainParameters[p.paramIndex] = p.value;
        anyChanged = true;
    }
    return anyChanged;
}

static const clap_plugin_descriptor_t pluginDescriptor = {
    .clap_version = CLAP_VERSION_INIT,
    .id = "naren.tuneBfree",
    .name = "tuneBfree",
    .vendor = "naren",
    .url = "https://github.com/narenratan/tuneBfree",
    .manual_url = "https://github.com/narenratan/tuneBfree",
    .support_url = "https://github.com/narenratan/tuneBfree",
    .version = "1.0.0",
    .description = "Tonewheel organ with microtuning",

    .features =
        (const char *[]){
            CLAP_PLUGIN_FEATURE_INSTRUMENT,
            CLAP_PLUGIN_FEATURE_SYNTHESIZER,
            CLAP_PLUGIN_FEATURE_STEREO,
            NULL,
        },
};

static const clap_plugin_note_ports_t extensionNotePorts = {
    .count = [](const clap_plugin_t *plugin, bool isInput) -> uint32_t { return isInput ? 1 : 0; },

    .get = [](const clap_plugin_t *plugin, uint32_t index, bool isInput,
              clap_note_port_info_t *info) -> bool {
        if (!isInput || index)
            return false;
        info->id = 0;
        info->supported_dialects = CLAP_NOTE_DIALECT_CLAP;
        info->preferred_dialect = CLAP_NOTE_DIALECT_CLAP;
        snprintf(info->name, sizeof(info->name), "%s", "Note Port");
        return true;
    },
};

static const clap_plugin_audio_ports_t extensionAudioPorts = {
    .count = [](const clap_plugin_t *plugin, bool isInput) -> uint32_t { return isInput ? 0 : 1; },

    .get = [](const clap_plugin_t *plugin, uint32_t index, bool isInput,
              clap_audio_port_info_t *info) -> bool {
        if (isInput || index)
            return false;
        info->id = 0;
        info->channel_count = 2;
        info->flags = CLAP_AUDIO_PORT_IS_MAIN;
        info->port_type = CLAP_PORT_STEREO;
        info->in_place_pair = CLAP_INVALID_ID;
        snprintf(info->name, sizeof(info->name), "%s", "Audio Output");
        return true;
    },
};

static const clap_plugin_params_t extensionParams = {
    .count = [](const clap_plugin_t *plugin) -> uint32_t { return P_COUNT; },

    .get_info = [](const clap_plugin_t *_plugin, uint32_t index,
                   clap_param_info_t *information) -> bool {
        if ((P_DRAWBAR_MIN <= index) && (index <= P_DRAWBAR_MAX))
        {
            memset(information, 0, sizeof(clap_param_info_t));
            information->id = index;
            information->flags = CLAP_PARAM_IS_AUTOMATABLE | CLAP_PARAM_IS_MODULATABLE;
            information->min_value = 0.0f;
            information->max_value = 8.0f;
            float default_drawbar_value[9] = {7.0, 8.0, 8.0, 0.0};
            information->default_value = default_drawbar_value[index];
            strcpy(information->name, ("Drawbar " + std::to_string(index)).c_str());
            return true;
        }
        else if (index == P_VIBRATO)
        {
            memset(information, 0, sizeof(clap_param_info_t));
            information->id = index;
            information->flags = CLAP_PARAM_IS_AUTOMATABLE | CLAP_PARAM_IS_MODULATABLE;
            information->min_value = 0.0f;
            information->max_value = 1.0f;
            information->default_value = 0.0f;
            strcpy(information->name, "Vibrato on/off");
            return true;
        }
        else if (index == P_VIBRATO_TYPE)
        {
            memset(information, 0, sizeof(clap_param_info_t));
            information->id = index;
            information->flags = CLAP_PARAM_IS_AUTOMATABLE | CLAP_PARAM_IS_MODULATABLE;
            information->min_value = 0.0f;
            information->max_value = 5.99f;
            information->default_value = 0.0f;
            strcpy(information->name, "Vibrato type");
            return true;
        }
        else if (index == P_DRUM)
        {
            memset(information, 0, sizeof(clap_param_info_t));
            information->id = index;
            information->flags = CLAP_PARAM_IS_AUTOMATABLE | CLAP_PARAM_IS_MODULATABLE;
            information->min_value = 0.0f;
            information->max_value = 2.99f;
            information->default_value = 1.0f;
            strcpy(information->name, "Drum");
            return true;
        }
        else if (index == P_HORN)
        {
            memset(information, 0, sizeof(clap_param_info_t));
            information->id = index;
            information->flags = CLAP_PARAM_IS_AUTOMATABLE | CLAP_PARAM_IS_MODULATABLE;
            information->min_value = 0.0f;
            information->max_value = 2.99f;
            information->default_value = 1.0f;
            strcpy(information->name, "Horn");
            return true;
        }
        else if (index == P_OVERDRIVE)
        {
            memset(information, 0, sizeof(clap_param_info_t));
            information->id = index;
            information->flags = CLAP_PARAM_IS_AUTOMATABLE | CLAP_PARAM_IS_MODULATABLE;
            information->min_value = 0.0f;
            information->max_value = 1.0f;
            information->default_value = 0.0f;
            strcpy(information->name, "Overdrive on/off");
            return true;
        }
        else if (index == P_CHARACTER)
        {
            memset(information, 0, sizeof(clap_param_info_t));
            information->id = index;
            information->flags = CLAP_PARAM_IS_AUTOMATABLE | CLAP_PARAM_IS_MODULATABLE;
            information->min_value = 0.0f;
            information->max_value = 1.0f;
            information->default_value = 0.0f;
            strcpy(information->name, "Character");
            return true;
        }
        else if (index == P_REVERB)
        {
            memset(information, 0, sizeof(clap_param_info_t));
            information->id = index;
            information->flags = CLAP_PARAM_IS_AUTOMATABLE | CLAP_PARAM_IS_MODULATABLE;
            information->min_value = 0.0f;
            information->max_value = 1.0f;
            information->default_value = 0.1f;
            strcpy(information->name, "Reverb wet/dry");
            return true;
        }
        else if (index == P_PERCUSSION)
        {
            memset(information, 0, sizeof(clap_param_info_t));
            information->id = index;
            information->flags = CLAP_PARAM_IS_AUTOMATABLE | CLAP_PARAM_IS_MODULATABLE;
            information->min_value = 0.0f;
            information->max_value = 1.0f;
            information->default_value = 0.0f;
            strcpy(information->name, "Percussion on/off");
            return true;
        }
        else if (index == P_PERCUSSION_VOLUME)
        {
            memset(information, 0, sizeof(clap_param_info_t));
            information->id = index;
            information->flags = CLAP_PARAM_IS_AUTOMATABLE | CLAP_PARAM_IS_MODULATABLE;
            information->min_value = 0.0f;
            information->max_value = 1.0f;
            information->default_value = 0.0f;
            strcpy(information->name, "Percussion soft/norm");
            return true;
        }
        else if (index == P_PERCUSSION_DECAY)
        {
            memset(information, 0, sizeof(clap_param_info_t));
            information->id = index;
            information->flags = CLAP_PARAM_IS_AUTOMATABLE | CLAP_PARAM_IS_MODULATABLE;
            information->min_value = 0.0f;
            information->max_value = 1.0f;
            information->default_value = 0.0f;
            strcpy(information->name, "Percussion fast/slow");
            return true;
        }
        else if (index == P_PERCUSSION_HARMONIC)
        {
            memset(information, 0, sizeof(clap_param_info_t));
            information->id = index;
            information->flags = CLAP_PARAM_IS_AUTOMATABLE | CLAP_PARAM_IS_MODULATABLE;
            information->min_value = 0.0f;
            information->max_value = 1.0f;
            information->default_value = 0.0f;
            strcpy(information->name, "Percussion 2nd/3rd");
            return true;
        }
        else
        {
            return false;
        }
    },

    .get_value = [](const clap_plugin_t *_plugin, clap_id id, double *value) -> bool {
        MyPlugin *plugin = (MyPlugin *)_plugin->plugin_data;
        uint32_t i = (uint32_t)id;
        if (i >= P_COUNT)
            return false;
        *value = plugin->parameters[i];
        return true;
    },

    .value_to_text =
        [](const clap_plugin_t *_plugin, clap_id id, double value, char *display, uint32_t size) {
            uint32_t i = (uint32_t)id;
            if (i >= P_COUNT)
                return false;
            snprintf(display, size, "%f", value);
            return true;
        },

    .text_to_value =
        [](const clap_plugin_t *_plugin, clap_id param_id, const char *display, double *value) {
            // TODO Implement this.
            return false;
        },

    .flush =
        [](const clap_plugin_t *_plugin, const clap_input_events_t *in,
           const clap_output_events_t *out) {
            MyPlugin *plugin = (MyPlugin *)_plugin->plugin_data;
            const uint32_t eventCount = in->size(in);
            PluginSyncMainToAudio(plugin, out);

            for (uint32_t eventIndex = 0; eventIndex < eventCount; eventIndex++)
            {
                PluginProcessEvent(plugin, in->get(in, eventIndex));
            }
        },
};

static const clap_plugin_state_t extensionState = {
    .save = [](const clap_plugin_t *_plugin, const clap_ostream_t *stream) -> bool {
        MyPlugin *plugin = (MyPlugin *)_plugin->plugin_data;
        PluginSyncAudioToMain(plugin);
        bool success = sizeof(float) * P_COUNT ==
                       stream->write(stream, plugin->mainParameters, sizeof(float) * P_COUNT);
        return success;
    },

    .load = [](const clap_plugin_t *_plugin, const clap_istream_t *stream) -> bool {
        MyPlugin *plugin = (MyPlugin *)_plugin->plugin_data;
        bool success = sizeof(float) * P_COUNT ==
                       stream->read(stream, plugin->mainParameters, sizeof(float) * P_COUNT);
        struct ParamMsg p;
        for (uint32_t i = 0; i < P_COUNT; i++)
        {
            p = {i, plugin->mainParameters[i]};
#ifdef DEBUG_PRINT
            fprintf(stderr, "extensionState.load toAudioQ.try_enqueue: %d %f\n", p.paramIndex,
                    p.value);
#endif
            toAudioQ.try_enqueue(p);
        }
        return success;
    },
};

static const clap_plugin_t pluginClass = {
    .desc = &pluginDescriptor,
    .plugin_data = nullptr,

    .init = [](const clap_plugin *_plugin) -> bool {
        MyPlugin *plugin = (MyPlugin *)_plugin->plugin_data;

        for (uint32_t i = 0; i < P_COUNT; i++)
        {
            clap_param_info_t information = {};
            extensionParams.get_info(_plugin, i, &information);
            plugin->mainParameters[i] = plugin->parameters[i] = information.default_value;
        }
        return true;
    },

    .destroy =
        [](const clap_plugin *_plugin) {
            MyPlugin *plugin = (MyPlugin *)_plugin->plugin_data;
            if (plugin->synth)
            {
                freeToneGenerator(plugin->synth);
            }
            if (plugin->preamp)
            {
                freePreamp((void *)plugin->preamp);
            }
            if (plugin->reverb)
            {
                freeReverb(plugin->reverb);
            }
            if (plugin->whirl)
            {
                freeWhirl(plugin->whirl);
            }
            free(plugin);
        },

    .activate = [](const clap_plugin *_plugin, double sampleRate, uint32_t minimumFramesCount,
                   uint32_t maximumFramesCount) -> bool {
        MyPlugin *plugin = (MyPlugin *)_plugin->plugin_data;
        plugin->sampleRate = sampleRate;

        plugin->synth = allocTonegen();
        initToneGenerator(plugin->synth, nullptr);
        init_vibrato(&(plugin->synth->inst_vibrato));
        plugin->whirl = allocWhirl();
        initWhirl(plugin->whirl, nullptr, sampleRate);
        void *preamp = allocPreamp();
        plugin->preamp = (struct b_preamp *)preamp;
        initPreamp(preamp, nullptr, sampleRate);
        plugin->reverb = allocReverb();
        initReverb(plugin->reverb, nullptr, sampleRate);
        return true;
    },

    .deactivate = [](const clap_plugin *_plugin) {},

    .start_processing = [](const clap_plugin *_plugin) -> bool { return true; },

    .stop_processing = [](const clap_plugin *_plugin) {},

    .reset =
        [](const clap_plugin *_plugin) { MyPlugin *plugin = (MyPlugin *)_plugin->plugin_data; },

    .process = [](const clap_plugin *_plugin,
                  const clap_process_t *process) -> clap_process_status {
        MyPlugin *plugin = (MyPlugin *)_plugin->plugin_data;

        assert(process->audio_outputs_count == 1);
        assert(process->audio_inputs_count == 0);

        const uint32_t frameCount = process->frames_count;
        const uint32_t inputEventCount = process->in_events->size(process->in_events);
        uint32_t eventIndex = 0;
        uint32_t nextEventFrame = inputEventCount ? 0 : frameCount;

        PluginSyncMainToAudio(plugin, process->out_events);

        for (uint32_t i = 0; i < frameCount;)
        {
            while (eventIndex < inputEventCount && nextEventFrame == i)
            {
                const clap_event_header_t *event =
                    process->in_events->get(process->in_events, eventIndex);

                if (event->time != i)
                {
                    nextEventFrame = event->time;
                    break;
                }

                PluginProcessEvent(plugin, event);
                eventIndex++;

                if (eventIndex == inputEventCount)
                {
                    nextEventFrame = frameCount;
                    break;
                }
            }

            PluginRenderAudio(plugin, i, nextEventFrame, process->audio_outputs[0].data32[0],
                              process->audio_outputs[0].data32[1]);
            i = nextEventFrame;
        }

        return CLAP_PROCESS_CONTINUE;
    },

    .get_extension = [](const clap_plugin *plugin, const char *id) -> const void * {
        if (0 == strcmp(id, CLAP_EXT_NOTE_PORTS))
            return &extensionNotePorts;
        if (0 == strcmp(id, CLAP_EXT_AUDIO_PORTS))
            return &extensionAudioPorts;
        if (0 == strcmp(id, CLAP_EXT_PARAMS))
            return &extensionParams;
        if (0 == strcmp(id, CLAP_EXT_STATE))
            return &extensionState;
        return nullptr;
    },

    .on_main_thread = [](const clap_plugin *_plugin) {},
};

static const clap_plugin_factory_t pluginFactory = {
    .get_plugin_count = [](const clap_plugin_factory *factory) -> uint32_t { return 1; },

    .get_plugin_descriptor = [](const clap_plugin_factory *factory, uint32_t index)
        -> const clap_plugin_descriptor_t * { return index == 0 ? &pluginDescriptor : nullptr; },

    .create_plugin = [](const clap_plugin_factory *factory, const clap_host_t *host,
                        const char *pluginID) -> const clap_plugin_t * {
        if (!clap_version_is_compatible(host->clap_version) ||
            strcmp(pluginID, pluginDescriptor.id))
        {
            return nullptr;
        }

        MyPlugin *plugin = (MyPlugin *)calloc(1, sizeof(MyPlugin));
        plugin->host = host;
        plugin->plugin = pluginClass;
        plugin->plugin.plugin_data = plugin;
        return &plugin->plugin;
    },
};

extern "C" const clap_plugin_entry_t clap_entry = {
    .clap_version = CLAP_VERSION_INIT,

    .init = [](const char *path) -> bool { return true; },

    .deinit = []() {},

    .get_factory = [](const char *factoryID) -> const void * {
        return strcmp(factoryID, CLAP_PLUGIN_FACTORY_ID) ? nullptr : &pluginFactory;
    },
};
