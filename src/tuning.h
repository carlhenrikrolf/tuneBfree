/*
 * Microtuning functions
 */

void getFrequencies(double *frequency, int length);
// Extend frequency[scaleLen .. length) by tiling the inferred period of the first
// scaleLen entries. scaleLen defaults to 128 (the pre-multichannel scale region);
// multichannel passes the gamut size.
void extendFrequencies(double *frequency, int length, int scaleLen = 128);
short getPairedWheel(short n);
// Infer scale size + period from frequency[0 .. scaleLen). scaleLen defaults to 128.
void inferScaleSize(double *frequency, int *scaleSizeRet, float *periodRet, int scaleLen = 128);

/**
 * Merge the 16×128 grid of per-(channel, note) fundamentals into one ascending,
 * de-duplicated "gamut" of the distinct pitches in use, and record which gamut
 * slot each (channel, note) maps to. This is the foundation of multichannel
 * microtuning: the channels address more notes of ONE scale, not different scales.
 * See roadmap/MULTICHANNEL.md.
 *
 *   freqGrid[c][n]   fundamental (Hz) for channel c, note n
 *   channelActive[c] whether channel c contributes (from the CHANNELS selection)
 *   noteMapped[c][n] false to exclude a (channel, note): a filtered/unmapped key
 *   gamutOut[]       receives the sorted unique pitches (caller sizes it >= 16*128)
 *   slotIndex[c][n]  receives the gamut index for (c, n), or -1 if excluded
 *   centsTolerance   pitches within this many cents collapse onto one slot/wheel
 *
 * Returns the gamut size (number of distinct pitches); 0 if nothing is active.
 */
int buildGamut(const double freqGrid[16][128],
               const bool   channelActive[16],
               const bool   noteMapped[16][128],
               double       gamutOut[],
               int          slotIndex[16][128],
               double       centsTolerance = 0.5);

/**
 * Equal-power crossfade weights for the keyboard split (step 2). A note at pitchHz is
 * routed to the lower manual below the split, the upper manual above it, and blended
 * across a crossfade zone of widthCents total, centred on splitHz. Equal-power means
 * wLower^2 + wUpper^2 == 1 (constant perceived loudness through the zone).
 *
 *   pitchHz    the note's sounding frequency
 *   splitHz    the split point (a frequency in the merged gamut)
 *   widthCents total width of the crossfade zone in cents (0 = hard split)
 *   wLower/wUpper receive the two gains in [0, 1]
 */
void splitCrossfade(double pitchHz, double splitHz, double widthCents,
                    double *wLower, double *wUpper);
