/**
 * OVERDRIVE
 *
 * In tuneBfree the original setBfree overdrive has been replaced with a version
 * of the wonderful Airwindows Density algorithm whose source is available here:
 *
 *   https://github.com/airwindows/airwindows/tree/master/plugins/LinuxVST/src/Density
 *
 * The Airwindows website https://www.airwindows.com/ and Patreon
 * https://www.patreon.com/airwindows are also hopefully of interest.
 *
 * Thank you Chris from Airwindows!
 *
 * The Density plugin itself is MIT licensed. The version used here is slightly
 * adapted - in any case I am including the license and copyright notice below.
 *
 ***************************************************************************************
 *      MIT License
 *
 *      Copyright (c) 2018 Chris Johnson
 *
 *      Permission is hereby granted, free of charge, to any person obtaining a copy
 *      of this software and associated documentation files (the "Software"), to deal
 *      in the Software without restriction, including without limitation the rights
 *      to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *      copies of the Software, and to permit persons to whom the Software is
 *      furnished to do so, subject to the following conditions:
 *
 *      The above copyright notice and this permission notice shall be included in all
 *      copies or substantial portions of the Software.
 *
 *      THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *      IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *      FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *      AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *      LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *      OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *      SOFTWARE.
 *
 ***************************************************************************************
 *
 */

#ifndef CONFIGDOCONLY
#include "overdrive.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * Airwindows Density algorithm
 *
 * See https://github.com/airwindows/airwindows/tree/master/plugins/LinuxVST/src/Density
 * and comment at the top of this file.
 *
 * Adapted here for mono and to wire into the setBfree b_preamp.
 */
float *airwindows_density(void *pa, float *in1, float *out1, int sampleFrames)
{
    struct b_preamp *pp = (struct b_preamp *)pa;

    float A = pp->A;
    float B = pp->B;
    float C = pp->C;
    float D = pp->D;

    double overallscale = 1.0;
    overallscale /= 44100.0;
    overallscale *= pp->SampleRateD;

    // Don't use negative density here
    // double density = (A*5.0)-1.0;
    double density = A * 4.0;

    double iirAmount = pow(B, 3) / overallscale;
    double output = C;
    double wet = D;
    double dry = 1.0 - wet;
    double bridgerectifier;
    double out = fabs(density);
    density = density * fabs(density);
    double count;

    double inputSampleL;
    double drySampleL;

    while (--sampleFrames >= 0)
    {
        inputSampleL = *in1;
        if (fabs(inputSampleL) < 1.18e-23)
            inputSampleL = pp->fpdL * 1.18e-17;
        drySampleL = inputSampleL;

        if (pp->fpFlip)
        {
            pp->iirSampleAL = (pp->iirSampleAL * (1.0 - iirAmount)) + (inputSampleL * iirAmount);
            inputSampleL -= pp->iirSampleAL;
        }
        else
        {
            pp->iirSampleBL = (pp->iirSampleBL * (1.0 - iirAmount)) + (inputSampleL * iirAmount);
            inputSampleL -= pp->iirSampleBL;
        }
        // highpass section
        pp->fpFlip = !pp->fpFlip;

        count = density;
        while (count > 1.0)
        {
            bridgerectifier = fabs(inputSampleL) * 1.57079633;
            if (bridgerectifier > 1.57079633)
                bridgerectifier = 1.57079633;
            // max value for sine function
            bridgerectifier = sin(bridgerectifier);
            if (inputSampleL > 0.0)
                inputSampleL = bridgerectifier;
            else
                inputSampleL = -bridgerectifier;

            count = count - 1.0;
        }
        // we have now accounted for any really high density settings.

        while (out > 1.0)
            out = out - 1.0;

        bridgerectifier = fabs(inputSampleL) * 1.57079633;
        if (bridgerectifier > 1.57079633)
            bridgerectifier = 1.57079633;
        // max value for sine function
        if (density > 0)
            bridgerectifier = sin(bridgerectifier);
        else
            bridgerectifier = 1 - cos(bridgerectifier);
        // produce either boosted or starved version
        if (inputSampleL > 0)
            inputSampleL = (inputSampleL * (1 - out)) + (bridgerectifier * out);
        else
            inputSampleL = (inputSampleL * (1 - out)) - (bridgerectifier * out);
        // blend according to density control

        if (output < 1.0)
        {
            inputSampleL *= output;
        }
        if (wet < 1.0)
        {
            inputSampleL = (drySampleL * dry) + (inputSampleL * wet);
        }
        // nice little output stage template: if we have another scale of floating point
        // number, we really don't want to meaninglessly multiply that by 1.0.

        // begin 32 bit floating point dither
        int expon;
        frexpf((float)inputSampleL, &expon);
        pp->fpdL ^= pp->fpdL << 13;
        pp->fpdL ^= pp->fpdL >> 17;
        pp->fpdL ^= pp->fpdL << 5;
        inputSampleL += ((double(pp->fpdL) - uint32_t(0x7fffffff)) * 5.5e-36l * pow(2, expon + 62));
        // end 32 bit floating point dither

        *out1 = inputSampleL;

        in1++;
        out1++;
    }
    return out1;
}

float *overdrive(void *pa, float *inBuf, float *outBuf, size_t buflen)
{
    return airwindows_density(pa, inBuf, outBuf, buflen);
}

#if 0
// Original setBfree overdrive
float *overdrive(void *pa, const float *inBuf, float *outBuf, size_t buflen)
{
    struct b_preamp *pp = (struct b_preamp *)pa;
    const float *xp = inBuf;
    float *yp = outBuf;
    int i;
    size_t n;

    for (n = 0; n < buflen; n++)
    {
        float xin;
        float u = 0.0;
        float v;
        float y = 0.0;

        /* Place the next input sample in the input history. */
        if (++(pp->xzp) == pp->xzpe)
        {
            pp->xzp = pp->xzb;
        }

        xin = pp->inputGain * (*xp++);
        pp->sagZ = (pp->sagFb * pp->sagZ) + fabsf(xin);
        pp->bias = pp->biasBase - (pp->sagZgb * pp->sagZ);
        pp->norm = 1.0 - (1.0 / (1.0 + (pp->bias * pp->bias)));
        *(pp->xzp) = xin;

        /* Check the input history wrap sentinel */
        if (pp->xzwp <= pp->xzp)
        {
            for (i = 0; i < 4; i++)
            {
                /* wp is ptr to interpol. filter weights for this sample */
                float *wp = &(pp->wi[i][0]);

                /* wpe is FIR weight end sentinel */
                float *wpe = wp + wiLen[i];

                /* xr is ptr to samples in input history */
                float *xr = pp->xzp;

                /* Apply convolution */
                while (wp < wpe)
                {
                    u += ((*wp++) * (*xr--));
                }
            }
        }
        else
        {
            /* Wrapping code */
            for (i = 0; i < 4; i++)
            {
                /* Interpolation weights for this sample */
                float *wp = &(pp->wi[i][0]);
                /* Weight end sentinel */
                float *wpe = wp + wiLen[i];
                /* Input history read pointer */
                float *xr = pp->xzp;

                while (pp->xzb <= xr)
                {
                    u += ((*wp++) * (*xr--));
                }

                xr = &(pp->xzb[63]);

                while (wp < wpe)
                {
                    u += ((*wp++) * (*xr--));
                }
            }
        }

        /* Apply transfer function */
        /* v = T (u); */
        /* Adaptive linear-non-linear transfer function */
        /* Global negative feedback */
        u -= (pp->adwGfb * pp->adwGfZ);
        {
            float temp = u - pp->adwZ;
            pp->adwZ = u + (pp->adwZ * pp->adwFb);
            u = temp;
        }
        if (u < 0.0)
        {
            float x2 = u - pp->bias;
            v = (1.0 / (1.0 + (x2 * x2))) - 1.0 + pp->norm;
        }
        else
        {
            float x2 = u + pp->bias;
            v = 1.0 - pp->norm - (1.0 / (1.0 + (x2 * x2)));
        }
        {
            float temp = v + (pp->adwFb2 * pp->adwZ1);
            v = temp - pp->adwZ1;
            pp->adwZ1 = temp;
        }
        /* Global negative feedback */
        pp->adwGfZ = v;

        /* Put transferred sample in output history. */
        if (++pp->yzp == pp->yzpe)
        {
            pp->yzp = pp->yzb;
        }
        *(pp->yzp) = v;

        /* Decimation */
        if (pp->yzwp <= pp->yzp)
        {
            /* No-wrap code */
            /* wp points to weights in the decimation FIR */
            float *wp = pp->aal;
            float *yr = pp->yzp;

            /* Convolve with decimation filter. */
            while (wp < pp->aalEnd)
            {
                y += ((*wp++) * (*yr--));
            }
        }
        else
        {
            /* Wrap code */
            float *wp = pp->aal;
            float *yr = pp->yzp;

            while (pp->yzb <= yr)
            {
                y += ((*wp++) * (*yr--));
            }

            yr = &(pp->yzb[127]);

            while (wp < pp->aalEnd)
            {
                y += ((*wp++) * (*yr--));
            }
        }

        *yp++ = pp->outputGain * y;
    }
    /* End of for-loop over input buffer */
    return outBuf;
} /* overdrive */
#endif

/* Adapter function */
float *preamp(void *pa, float *inBuf, float *outBuf, size_t bufLengthSamples)
{
    struct b_preamp *pp = (struct b_preamp *)pa;
    if (pp->isClean)
    {
        memcpy(outBuf, inBuf, bufLengthSamples * sizeof(float));
    }
    else
    {
        overdrive(pa, inBuf, outBuf, bufLengthSamples);
    }

    return outBuf;
}

void *allocPreamp()
{
    struct b_preamp *pp = (struct b_preamp *)calloc(1, sizeof(struct b_preamp));

    pp->A = 0.0;
    pp->B = 0.0;
    pp->C = 1.0;
    pp->D = 0.5;
    pp->iirSampleAL = 0.0;
    pp->iirSampleBL = 0.0;
    pp->fpFlip = true;
    uint32_t fpdL = 1.0;
    while (fpdL < 16386)
        fpdL = rand() * UINT32_MAX;
    pp->fpdL = fpdL;

    pp->isClean = 1;
    pp->outputGain = 0.8795;
    pp->inputGain = 3.5675;

    pp->SampleRateD = 0.0;

    pp->sagZ = 0.0;
    pp->sagFb = 0.991;
    pp->sagFb = 0.991;
    pp->biasBase = 0.5347;
    pp->adwZ = 0.0;
    pp->adwFb = 0.5821;
    pp->adwZ1 = 0.0;
    pp->adwFb2 = 0.999;
    pp->adwGfb = -0.6214;
    pp->adwGfZ = 0.0;
    pp->sagZgb = 0.0094;
    return pp;
}

void freePreamp(void *pa)
{
    struct b_preamp *pp = (struct b_preamp *)pa;
    free(pp);
}

#ifndef CLAP
void setClean(void *pa, int useClean)
{
    struct b_preamp *pp = (struct b_preamp *)pa;
    pp->isClean = useClean ? 1 : 0;
}
void setCleanCC(void *pa, unsigned char uc) { setClean(pa, uc > 63 ? 0 : 1); }

/* Legacy function */
int ampConfig(void *pa, ConfigContext *cfg)
{
    struct b_preamp *pp = (struct b_preamp *)pa;
    int rtn = 1;
    float v = 0;

    /* Config generated by overmaker */

    if (getConfigParameter_f("overdrive.inputgain", cfg, &pp->inputGain))
        return 1;
    else if (getConfigParameter_f("overdrive.outputgain", cfg, &pp->outputGain))
        return 1;
    else if (getConfigParameter_f("xov.ctl_biased_gfb", cfg, &v))
    {
        fctl_biased_gfb(pp, v);
        return 1;
    }
    else if (getConfigParameter_f("xov.ctl_biased", cfg, &v))
    {
        fctl_biased(pp, v);
        return 1;
    }
    else if (getConfigParameter_f("overdrive.character", cfg, &v))
    {
        fctl_biased_fat(pp, v);
        return 1;
    }

    /* Config generated by external module */

    if (getConfigParameter_fr("xov.ctl_biased_fb", cfg, &pp->adwFb, 0, 0.999))
        ;
    else if (getConfigParameter_fr("xov.ctl_biased_fb2", cfg, &pp->adwFb2, 0, 0.999))
        ;
    else if (getConfigParameter_f("xov.ctl_sagtobias", cfg, &pp->sagFb))
        ;
    else
        return 0;
    return rtn;
}

/* Computes the constants for transfer curve */
void cfg_biased(void *pa, float new_bias)
{
    struct b_preamp *pp = (struct b_preamp *)pa;
    if (0.0 < new_bias)
    {
        pp->biasBase = new_bias;
        /* If power sag emulation is enabled bias is set there. */
        pp->bias = pp->biasBase;
        pp->norm = 1.0 - (1.0 / (1.0 + (pp->bias * pp->bias)));
    }
}
void fctl_biased(void *pa, float u)
{
    float v = 0 + ((0.7 - 0) * (u * u));
    cfg_biased(pa, v);
}

void ctl_biased(void *d, unsigned char uc) { fctl_biased(d, uc / 127.0); }

/* ovt_biased:Sets the positive feedback */
void fctl_biased_fb(void *pa, float u)
{
    struct b_preamp *pp = (struct b_preamp *)pa;
    pp->adwFb = 0.999 * u;
    printf("\rFbk=%10.4f", pp->adwFb);
    fflush(stdout);
}

void ctl_biased_fb(void *d, unsigned char uc) { fctl_biased_fb(d, uc / 127.0); }

/* ovt_biased: Sets sag impact */
void fctl_sagtoBias(void *pa, float u)
{
    struct b_preamp *pp = (struct b_preamp *)pa;
    pp->sagZgb = 0 + ((0.05 - 0) * u);
    printf("\rpp->ZGB=%10.4f", pp->sagZgb);
    fflush(stdout);
}

void ctl_sagtoBias(void *d, unsigned char uc) { fctl_sagtoBias(d, uc / 127.0); }

/* ovt_biased: Postdiff feedback control */
void fctl_biased_fb2(void *pa, float u)
{
    struct b_preamp *pp = (struct b_preamp *)pa;
    pp->adwFb2 = 0.999 * u;
    printf("\rFb2=%10.4f", pp->adwFb2);
    fflush(stdout);
}

void ctl_biased_fb2(void *d, unsigned char uc) { fctl_biased_fb2(d, uc / 127.0); }

/* ovt_biased: Global feedback control */
void fctl_biased_gfb(void *pa, float u)
{
    struct b_preamp *pp = (struct b_preamp *)pa;
    pp->adwGfb = -0.999 * u;
    printf("\rGfb=%10.4f", pp->adwGfb);
    fflush(stdout);
}

void ctl_biased_gfb(void *d, unsigned char uc) { fctl_biased_gfb(d, uc / 127.0); }

/* ovt_biased: Fat control */
void ctl_biased_fat(void *pa, unsigned char uc)
{
    struct b_preamp *pp = (struct b_preamp *)pa;
    if (uc < 64)
    {
        if (uc < 32)
        {
            pp->adwFb = 0.5821;
            pp->adwFb2 = 0.999 + ((0.5821 - 0.999) * (((float)uc) / 31.0));
        }
        else
        {
            pp->adwFb = 0.5821 + ((0.999 - 0.5821) * (((float)(uc - 32)) / 31.0));
            pp->adwFb2 = 0.5821;
        }
    }
    else
    {
        pp->adwFb = 0.999;
        pp->adwFb2 = 0.5821 + ((0.999 - 0.5821) * (((float)(uc - 64)) / 63.0));
    }
}

void fctl_biased_fat(void *d, float f) { ctl_biased_fat(d, (unsigned char)(f * 127.0)); }

void setInputGain(void *pa, unsigned char uc)
{
    struct b_preamp *pp = (struct b_preamp *)pa;
    pp->inputGain = 0.001 + ((10 - 0.001) * (((float)uc) / 127.0));
    printf("\rINP:%10.4lf", pp->inputGain);
    fflush(stdout);
}

void fsetInputGain(void *d, float f) { setInputGain(d, (unsigned char)(f * 127.0)); }

void setOutputGain(void *pa, unsigned char uc)
{
    struct b_preamp *pp = (struct b_preamp *)pa;
    pp->outputGain = 0.1 + ((10 - 0.1) * (((float)uc) / 127.0));
    printf("\rOUT:%10.4lf", pp->outputGain);
    fflush(stdout);
}

void fsetOutputGain(void *d, float f) { setOutputGain(d, (unsigned char)(f * 127.0)); }
#endif

float linseg(float a, float b, float p, float q, float x)
{
    return p + (x - a) * (q - p) / (b - a);
}

void fsetCharacter(struct b_preamp *pp, float A)
{
    pp->A = A;

    /*
     * A is used as the Density parameter in Airwindows Density
     * C is used as the Out-level parameter
     * The setBfree GUI has one knob (Character/Gain) mapped to A
     * Increasing A gives more overdrive and also more volume, so we lower C to compensate
     * The curve is chosen so output volume sounds roughly constant as A increases.
     */
    double Aval[5] = {0.0, 0.25, 0.50, 0.75, 1.00};
    double Cval[5] = {1.0, 0.70, 0.25, 0.15, 0.13};
    int i;
    for (i = 0; i < 4; i++)
    {
        if (A <= Aval[i + 1])
        {
            pp->C = linseg(Aval[i], Aval[i + 1], Cval[i], Cval[i + 1], A);
            return;
        }
    }
}

void setCharacter(void *pa, unsigned char uc)
{
    struct b_preamp *pp = (struct b_preamp *)pa;
    fsetCharacter(pp, 0.001 + ((1.0 - 0.001) * (((float)uc) / 127.0)));
}

/* Legacy function */
void initPreamp(void *pa, void *m, double SampleRateD)
{
    struct b_preamp *pp = (struct b_preamp *)pa;
    pp->SampleRateD = SampleRateD;
#ifndef CLAP
    useMIDIControlFunction(m, "xov.ctl_biased", ctl_biased, pa);
    useMIDIControlFunction(m, "xov.ctl_biased_fb", ctl_biased_fb, pa);
    useMIDIControlFunction(m, "xov.ctl_biased_fb2", ctl_biased_fb2, pa);
    useMIDIControlFunction(m, "xov.ctl_biased_gfb", ctl_biased_gfb, pa);
    useMIDIControlFunction(m, "xov.ctl_sagtobias", ctl_sagtoBias, pa);
    useMIDIControlFunction(m, "overdrive.character", setCharacter, pa);
    cfg_biased(pa, 0.5347);
    pp->adwFb = 0.5821;
    useMIDIControlFunction(m, "overdrive.enable", setCleanCC, pa);
    useMIDIControlFunction(m, "overdrive.inputgain", setInputGain, pa);
    useMIDIControlFunction(m, "overdrive.outputgain", setOutputGain, pa);
#endif
}
#else // no CONFIGDOCONLY
#include "cfgParser.h"
#endif

#ifndef CLAP
static const ConfigDoc doc[] = {
    {"overdrive.inputgain", CFG_FLOAT, "0.3567",
     "This is how much the input signal is scaled as it enters the overdrive "
     "effect. The default "
     "value is quite hot, but you can of course try it in anyway you like; "
     "range [0..1]",
     INCOMPLETE_DOC},
    {"overdrive.outputgain", CFG_FLOAT, "0.07873",
     "This is how much the signal is scaled as it leaves the overdrive effect. "
     "Essentially this "
     "value should be as high as possible without clipping (and you *will* "
     "notice when it does - "
     "Test with a bass-chord on 88 8888 000 with percussion enabled and full "
     "swell, but do turn "
     "down the amplifier/headphone volume first!); range [0..1]",
     INCOMPLETE_DOC},
    {"xov.ctl_biased", CFG_FLOAT, "0.5347", "bias base; range [0..1]", INCOMPLETE_DOC},
    {"xov.ctl_biased_gfb", CFG_FLOAT, "0.6214", "Global [negative] feedback control; range [0..1]",
     INCOMPLETE_DOC},
    {"overdrive.character", CFG_FLOAT, "-",
     "Abstraction to set xov.ctl_biased_fb and xov.ctl_biased_fb2", INCOMPLETE_DOC},
    {"xov.ctl_biased_fb", CFG_FLOAT, "0.5821",
     "This parameter behaves somewhat like an analogue tone control for bass "
     "mounted before the "
     "overdrive stage. Unity is somewhere around the value 0.6, lesser values "
     "takes away bass and "
     "lowers the volume while higher values gives more bass and more signal "
     "into the overdrive. "
     "Must be less than 1.0.",
     INCOMPLETE_DOC},
    {"xov.ctl_biased_fb2", CFG_FLOAT, "0.999",
     "The fb2 parameter has the same function as fb1 but controls the signal "
     "after the overdrive "
     "stage. Together the two parameters are useful in that they can reduce "
     "the amount of bass "
     "going into the overdrive and then recover it on the other side. Must be "
     "less than 1.0.",
     INCOMPLETE_DOC},
    {"xov.ctl_sagtobias", CFG_FLOAT, "0.1880",
     "This parameter is part of an attempt to recreate an artefact called "
     "'power sag'. When a "
     "power amplifier is under heavy load the voltage drops and alters the "
     "operating parameters of "
     "the unit, usually towards more and other kinds of distortion. The sagfb "
     "parameter controls "
     "the rate of recovery from the sag effect when the load is lifted. Must "
     "be less than 1.0.",
     INCOMPLETE_DOC},
    DOC_SENTINEL};

const ConfigDoc *ampDoc() { return doc; }
#endif
