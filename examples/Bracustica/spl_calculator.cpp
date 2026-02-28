#include "spl_calculator.h"
#include "config.h"
#include <math.h>

// A-weighting biquad coefficients for 8 kHz — same as PiezoTest
static Biquad aw1 = { 0.25574113f, -0.51148225f, 0.25574113f, -0.64730764f, 0.1429805f,  0, 0 };
static Biquad aw2 = { 0.20657208f,  0.41314417f, 0.20657208f, -0.36952738f, 0.19581571f, 0, 0 };

static float applyBiquad(Biquad &f, float x) {
    float y = f.b0 * x + f.z1;
    f.z1    = f.b1 * x - f.a1 * y + f.z2;
    f.z2    = f.b2 * x - f.a2 * y;
    return y;
}

void resetAWeighting() {
    aw1.z1 = aw1.z2 = 0;
    aw2.z1 = aw2.z2 = 0;
}

void applyAWeighting(const int16_t *in, float *out, int len) {
    for (int i = 0; i < len; i++) {
        float s = in[i] / 32768.0f;
        out[i]  = applyBiquad(aw2, applyBiquad(aw1, s));
    }
}

float computeRMS(const float *samples, int len) {
    if (len == 0) return 0.0f;
    double sum = 0;
    for (int i = 0; i < len; i++) sum += (double)samples[i] * samples[i];
    return sqrtf((float)(sum / len));
}

// Converts RMS of normalised float samples to dB SPL (dBFS-based estimate).
// 0 dBFS full-scale sine ≈ 94 dB SPL for a calibrated mic — adjust via config.
float rmsToDBSPL(float rms) {
    if (rms <= 0.0f) return -120.0f;
    return 20.0f * log10f(rms / SPL_REF_PA);
}
