#pragma once
#include <Arduino.h>

// A-weighting biquad state (8 kHz coefficients, same as PiezoTest)
struct Biquad {
    float b0, b1, b2, a1, a2;
    float z1, z2;
};

void   resetAWeighting();
void   applyAWeighting(const int16_t *in, float *out, int len);
float  computeRMS(const float *samples, int len);
float  rmsToDBSPL(float rms);
