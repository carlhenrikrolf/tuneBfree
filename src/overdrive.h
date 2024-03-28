/* setBfree - DSP tonewheel organ
 *
 * Copyright (C) 2003-2004 Fredrik Kilander <fk@dsv.su.se>
 * Copyright (C) 2008-2018 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2012 Will Panther <pantherb@setbfree.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _OVERDRIVE_H_
#define _OVERDRIVE_H_

#include "../src/cfgParser.h"
#include "../src/midi.h" // useMIDIControlFunction
extern int ampConfig (void* pa, ConfigContext* cfg);
extern const ConfigDoc* ampDoc ();

extern void initPreamp (void* pa, void* m, double SampleRateD);
extern void setClean (void* pa, int useClean);

extern void* allocPreamp ();
extern void freePreamp (void* pa);

extern float* preamp (void* pa, float* inBuf, float* outBuf, size_t bufLengthSamples);
extern float* overdrive (void* pa, const float* inBuf, float* outBuf, size_t buflen);

struct b_preamp
{
    // For Airwindows Density
    double iirSampleAL;
    double iirSampleBL;
    bool fpFlip;
    uint32_t fpdL;
    // parameters. Always 0-1, and we scale/alter them elsewhere.
    float A; // Density
    float B; // Highpass
    float C; // Out Level
    float D; // Dry/Wet

    /* Clean/overdrive switch */
    int isClean;
    float outputGain;

    double SampleRateD;

    /* Input gain */
    float inputGain;
    float sagZ;
    float sagFb;
    /* Variables for the inverted and biased transfer function */
    float biasBase;
    /* bias and norm are set in function cfg_biased() */
    float bias;
    float norm;
    /* ovt_biased : One sample memory */
    float adwZ;
    /* ovt_biased : Positive feedback */
    float adwFb;

    float adwZ1;
    float adwFb2;
    float adwGfb;
    float adwGfZ;
    float sagZgb;
};
/*  *** END STRUCT *** */

/* the following depend on compile time configutaion
 * and should be created by overmaker
 */

/** Computes the constants for transfer curve */
void fctl_biased (void* d, float u);
/** ovt_biased:Sets the positive feedback */
void fctl_biased_fb (void* d, float u);
/** ovt_biased: Sets sag impact */
void fctl_sagtoBias (void* d, float u);
/** ovt_biased: Postdiff feedback control */
void fctl_biased_fb2 (void* d, float u);
/** ovt_biased: Global feedback control */
void fctl_biased_gfb (void* d, float u);
/** ovt_biased: Fat control */
void fctl_biased_fat (void* d, float u);

void fsetInputGain (void* d, float u);
void fsetOutputGain (void* d, float u);
void fsetCharacter(struct b_preamp* d, float A);

#endif /* _OVERDRIVE_H_ */
