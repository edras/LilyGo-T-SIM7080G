#pragma once
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "config.h"

// Circular buffer in PSRAM holding RING_BUFFER_SECONDS of ADPCM audio.
// Each slot = ADPCM_BYTES_PER_SEC bytes (one second of audio).
// Thread-safe via mutex.

bool  audioRingInit();
void  audioRingPushSecond(const uint8_t *adpcmSec, size_t bytes);
// Copy last `seconds` seconds of audio into `out` (must be >= seconds * ADPCM_BYTES_PER_SEC).
// Returns actual bytes copied.
size_t audioRingGetLast(int seconds, uint8_t *out);
