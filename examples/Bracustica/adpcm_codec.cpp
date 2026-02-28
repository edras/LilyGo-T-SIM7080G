#include "adpcm_codec.h"
#include <Arduino.h>

static const int stepTable[89] = {
    7,8,9,10,11,12,13,14,16,17,19,21,23,25,28,31,34,37,41,45,50,
    55,60,66,73,80,88,97,107,118,130,143,157,173,190,209,230,253,
    279,307,337,371,408,449,494,544,598,658,724,796,876,963,1060,
    1166,1282,1411,1552,1707,1878,2066,2272,2499,2749,3024,3327,
    3660,4026,4428,4871,5358,5894,6484,7132,7845,8630,9493,10442,
    11487,12635,13899,15289,16818,18500,20350,22385,24623,27086,29794,32767
};

static const int indexTable[8] = { -1,-1,-1,-1,2,4,6,8 };

static int clamp(int v, int lo, int hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

void adpcmReset(ADPCMState &s) {
    s.predicted  = 0;
    s.stepIndex  = 0;
}

static uint8_t encodeSample(ADPCMState &s, int16_t sample) {
    int step  = stepTable[s.stepIndex];
    int diff  = sample - s.predicted;
    uint8_t code = 0;
    if (diff < 0) { code = 8; diff = -diff; }
    int delta = 0;
    if (diff >= step)     { code |= 4; diff -= step; delta += step; }
    step >>= 1;
    if (diff >= step)     { code |= 2; diff -= step; delta += step; }
    step >>= 1;
    if (diff >= step)     { code |= 1;               delta += step; }
    delta += step >> 1;
    s.predicted = (int16_t)clamp(s.predicted + ((code & 8) ? -delta : delta), -32768, 32767);
    s.stepIndex = clamp(s.stepIndex + indexTable[code & 7], 0, 88);
    return code & 0x0F;
}

static int16_t decodeSample(ADPCMState &s, uint8_t code) {
    int step  = stepTable[s.stepIndex];
    int delta = (step >> 3);
    if (code & 4) delta += step;
    if (code & 2) delta += (step >> 1);
    if (code & 1) delta += (step >> 2);
    s.predicted = (int16_t)clamp(s.predicted + ((code & 8) ? -delta : delta), -32768, 32767);
    s.stepIndex = clamp(s.stepIndex + indexTable[code & 7], 0, 88);
    return s.predicted;
}

size_t adpcmEncode(ADPCMState &s, const int16_t *pcm, size_t sampleCount, uint8_t *out) {
    size_t bytes = 0;
    for (size_t i = 0; i + 1 < sampleCount; i += 2) {
        uint8_t lo = encodeSample(s, pcm[i]);
        uint8_t hi = encodeSample(s, pcm[i + 1]);
        out[bytes++] = lo | (hi << 4);
    }
    if (sampleCount & 1) {
        out[bytes++] = encodeSample(s, pcm[sampleCount - 1]);
    }
    return bytes;
}

size_t adpcmDecode(ADPCMState &s, const uint8_t *adpcm, size_t byteCount, int16_t *out) {
    size_t n = 0;
    for (size_t i = 0; i < byteCount; i++) {
        out[n++] = decodeSample(s, adpcm[i] & 0x0F);
        out[n++] = decodeSample(s, (adpcm[i] >> 4) & 0x0F);
    }
    return n;
}
