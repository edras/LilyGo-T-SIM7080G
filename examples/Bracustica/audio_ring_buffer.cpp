#include "audio_ring_buffer.h"

static uint8_t *ring    = nullptr;
static int      head    = 0;   // next slot to write
static int      count   = 0;   // filled slots (max RING_BUFFER_SECONDS)
static SemaphoreHandle_t mtx = nullptr;

bool audioRingInit() {
    ring = (uint8_t *)ps_malloc((size_t)RING_BUFFER_SECONDS * ADPCM_BYTES_PER_SEC);
    if (!ring) return false;
    mtx = xSemaphoreCreateMutex();
    return mtx != nullptr;
}

void audioRingPushSecond(const uint8_t *adpcmSec, size_t bytes) {
    if (!ring || bytes == 0) return;
    size_t toCopy = (bytes < ADPCM_BYTES_PER_SEC) ? bytes : ADPCM_BYTES_PER_SEC;
    xSemaphoreTake(mtx, portMAX_DELAY);
    memcpy(ring + (size_t)head * ADPCM_BYTES_PER_SEC, adpcmSec, toCopy);
    head  = (head + 1) % RING_BUFFER_SECONDS;
    if (count < RING_BUFFER_SECONDS) count++;
    xSemaphoreGive(mtx);
}

size_t audioRingGetLast(int seconds, uint8_t *out) {
    if (!ring || seconds <= 0) return 0;
    xSemaphoreTake(mtx, portMAX_DELAY);
    int avail = (seconds < count) ? seconds : count;
    // oldest slot of the window
    int start = (head - avail + RING_BUFFER_SECONDS) % RING_BUFFER_SECONDS;
    size_t copied = 0;
    for (int i = 0; i < avail; i++) {
        int slot = (start + i) % RING_BUFFER_SECONDS;
        memcpy(out + copied, ring + (size_t)slot * ADPCM_BYTES_PER_SEC, ADPCM_BYTES_PER_SEC);
        copied += ADPCM_BYTES_PER_SEC;
    }
    xSemaphoreGive(mtx);
    return copied;
}
