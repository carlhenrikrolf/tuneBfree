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

#include <stddef.h>
#include <cstdint>

#ifndef REVERB_H
#define REVERB_H

#define RV_NZ 7
struct b_reverb {
    b_reverb();
    float *reverb(float *inbuf, float *outbuf, int bufferLengthSamples);

    double biquadA[11];
    double biquadB[11];
    double biquadC[11];

    double aAL[8111];
    double aBL[7511];
    double aCL[7311];
    double aDL[6911];
    double aEL[6311];
    double aFL[6111];
    double aGL[5511];
    double aHL[4911];
    double aIL[4511];
    double aJL[4311];
    double aKL[3911];
    double aLL[3311];
    double aML[3111];

    double aAR[8111];
    double aBR[7511];
    double aCR[7311];
    double aDR[6911];
    double aER[6311];
    double aFR[6111];
    double aGR[5511];
    double aHR[4911];
    double aIR[4511];
    double aJR[4311];
    double aKR[3911];
    double aLR[3311];
    double aMR[3111];

    int countA, delayA;
    int countB, delayB;
    int countC, delayC;
    int countD, delayD;
    int countE, delayE;
    int countF, delayF;
    int countG, delayG;
    int countH, delayH;
    int countI, delayI;
    int countJ, delayJ;
    int countK, delayK;
    int countL, delayL;
    int countM, delayM;

    double feedbackAL, vibAL, depthA;
    double feedbackBL, vibBL, depthB;
    double feedbackCL, vibCL, depthC;
    double feedbackDL, vibDL, depthD;
    double feedbackEL, vibEL, depthE;
    double feedbackFL, vibFL, depthF;
    double feedbackGL, vibGL, depthG;
    double feedbackHL, vibHL, depthH;

    double feedbackAR, vibAR;
    double feedbackBR, vibBR;
    double feedbackCR, vibCR;
    double feedbackDR, vibDR;
    double feedbackER, vibER;
    double feedbackFR, vibFR;
    double feedbackGR, vibGR;
    double feedbackHR, vibHR;

    uint32_t fpdL;
    uint32_t fpdR;
    //default stuff

    float A;
    float B;
    float C;
    float D;
    float E;
    float F;
    float G;

    /* static config */
    double SampleRateD;
};

#include "../src/cfgParser.h"
extern struct b_reverb* allocReverb ();
void freeReverb (struct b_reverb* r);

extern int reverbConfig (struct b_reverb* r, ConfigContext* cfg);

extern const ConfigDoc* reverbDoc ();

extern void setReverbMix (struct b_reverb* r, float g);

extern void initReverb (struct b_reverb* r, void* m, double rate);

#endif /* REVERB_H */
