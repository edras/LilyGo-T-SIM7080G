#pragma once
#include <Arduino.h>

// IMA ADPCM: 4:1 compression (16-bit PCM → 4-bit codes, 2 per byte)
// Stateful streaming encoder — call encodeReset() before a new stream.

struct ADPCMState {
    int16_t predicted;
    int      stepIndex;
};

void   adpcmReset(ADPCMState &s);
// Encode `sampleCount` PCM samples → `out` (must be >= sampleCount/2 bytes).
// Returns bytes written.
size_t adpcmEncode(ADPCMState &s, const int16_t *pcm, size_t sampleCount, uint8_t *out);
// Decode `byteCount` ADPCM bytes → `out` PCM (must be >= byteCount*2 samples).
// Returns samples written.
size_t adpcmDecode(ADPCMState &s, const uint8_t *adpcm, size_t byteCount, int16_t *out);
