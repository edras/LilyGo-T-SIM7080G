#include <Arduino.h>
#include "config.h"
#include "audio_capture.h"
#include "spl_calculator.h"
#include "adpcm_codec.h"
#include "audio_ring_buffer.h"
#include "sd_storage.h"
#include "power_manager.h"
#include "modem_client.h"
#include "gps_manager.h"
#include "display_debug.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>

// ── Event bits ───────────────────────────────────────────────────
#define EVT_THRESHOLD_CROSSED  BIT0
#define EVT_TRANSMIT_SPL       BIT1

static EventGroupHandle_t events;

// ── Shared state (written by audioTask, read by others) ──────────
static float  splCircular[RING_BUFFER_SECONDS] = {};
static int    splHead  = 0;   // next write index
static int    splCount = 0;
static float  lastSPL  = 0;
static SemaphoreHandle_t splMtx;

// ── Buffers (stack/static to avoid heap fragmentation) ───────────
static int16_t sampleAccum[SAMPLES_PER_SECOND];
static float   filteredBuf[SAMPLES_PER_SECOND];
static uint8_t adpcmSecBuf[ADPCM_BYTES_PER_SEC];
static uint8_t alertBuf[ALERT_BUFFER_SIZE];   // 40 KB, static in PSRAM? use ps_malloc if needed

// ── audioTask ────────────────────────────────────────────────────
static void audioTask(void *) {
    ADPCMState encState;
    adpcmReset(encState);
    resetAWeighting();

    size_t accumulated = 0;

    while (true) {
        size_t got = readAudioSamples(sampleAccum + accumulated,
                                      SAMPLES_PER_SECOND - accumulated,
                                      pdMS_TO_TICKS(200));
        accumulated += got;

        if (accumulated < SAMPLES_PER_SECOND) continue;
        accumulated = 0;

        // ── Compute A-weighted SPL ────────────────────────────────
        applyAWeighting(sampleAccum, filteredBuf, SAMPLES_PER_SECOND);
        float rms = computeRMS(filteredBuf, SAMPLES_PER_SECOND);
        float spl = rmsToDBSPL(rms);

        // ── Push SPL to circular array ────────────────────────────
        xSemaphoreTake(splMtx, portMAX_DELAY);
        splCircular[splHead] = spl;
        splHead = (splHead + 1) % RING_BUFFER_SECONDS;
        if (splCount < RING_BUFFER_SECONDS) splCount++;
        lastSPL = spl;
        xSemaphoreGive(splMtx);

        // ── ADPCM encode and push to audio ring buffer ───────────
        size_t bytes = adpcmEncode(encState, sampleAccum, SAMPLES_PER_SECOND, adpcmSecBuf);
        audioRingPushSecond(adpcmSecBuf, bytes);

        // ── Threshold check ───────────────────────────────────────
        if (spl >= SPL_THRESHOLD_DB) {
            xEventGroupSetBits(events, EVT_THRESHOLD_CROSSED);
        }

        // ── SD log (non-blocking best-effort) ────────────────────
        GPSData gps = gpsGetCached();
        sdLogSPL(millis() / 1000, spl, gps.lat, gps.lon);

        Serial.printf("[Audio] SPL=%.1f dB  RMS=%.5f\n", spl, rms);
    }
}

// ── transmitTask ──────────────────────────────────────────────────
static void transmitTask(void *) {
    static bool modemReady = false;
    static uint32_t lastGPSUpdate = 0;

    // Wait a bit for audio to stabilise before first TX attempt
    vTaskDelay(pdMS_TO_TICKS(10000));

    if (!modemInit()) {
        Serial.println("[TX] Modem init failed — transmit disabled");
        vTaskDelete(nullptr);
        return;
    }
    if (!modemConnect()) {
        Serial.println("[TX] Network registration failed");
        vTaskDelete(nullptr);
        return;
    }
    modemReady = true;
    modemDisconnect();

    while (true) {
        // Wait for SPL transmit tick or alert
        EventBits_t bits = xEventGroupWaitBits(events,
            EVT_TRANSMIT_SPL | EVT_THRESHOLD_CROSSED,
            pdTRUE, pdFALSE,
            pdMS_TO_TICKS(SPL_TRANSMIT_INTERVAL_S * 1000UL));

        if (!modemReady) continue;

        // ── GPS every 30 min ──────────────────────────────────────
        uint32_t nowS = millis() / 1000;
        if (nowS - lastGPSUpdate >= GPS_INTERVAL_S) {
            gpsTryFix();
            lastGPSUpdate = nowS;
        }

        GPSData gps = gpsGetCached();
        float   battV = getBatteryVoltage();

        // ── Audio alert (priority) ────────────────────────────────
        if (bits & EVT_THRESHOLD_CROSSED) {
            size_t alertBytes = audioRingGetLast(ALERT_SECONDS, alertBuf);
            float  peak       = lastSPL;
            displayAlert(peak);
            sdSaveAlert(millis() / 1000, alertBuf, alertBytes);
            if (modemConnect()) {
                sendAudioAlert(alertBuf, alertBytes, peak, gps, millis() / 1000);
                modemDisconnect();
            }
        }

        // ── SPL report every 60 s ────────────────────────────────
        if (bits & EVT_TRANSMIT_SPL || true) {  // always send on each wake
            float snapshot[RING_BUFFER_SECONDS];
            int   n;
            xSemaphoreTake(splMtx, portMAX_DELAY);
            n = splCount;
            for (int i = 0; i < n; i++) {
                int idx = (splHead - n + i + RING_BUFFER_SECONDS) % RING_BUFFER_SECONDS;
                snapshot[i] = splCircular[idx];
            }
            xSemaphoreGive(splMtx);

            if (modemConnect()) {
                sendSPLReport(snapshot, n, gps, millis() / 1000);
                modemDisconnect();
            }
        }

        displayStatus(lastSPL, battV, modemReady, true);
    }
}

// ── SPL tick task — fires EVT_TRANSMIT_SPL every 60 s ────────────
static void splTickTask(void *) {
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(SPL_TRANSMIT_INTERVAL_S * 1000UL));
        xEventGroupSetBits(events, EVT_TRANSMIT_SPL);
    }
}

// ── setup / loop ─────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    while (!Serial);
    delay(1000);
    Serial.println("[Bracustica] Boot");

    events  = xEventGroupCreate();
    splMtx  = xSemaphoreCreateMutex();

    initPMU();
    enableSDPower();
    initDisplay();
    initSD();
    initAudio();
    gpsManagerInit();
    audioRingInit();
    enableLightSleep();

    displayStatus(0, 0, false, true);

    xTaskCreatePinnedToCore(audioTask,    "audio",    8192, nullptr, 5, nullptr, 1);
    xTaskCreatePinnedToCore(transmitTask, "transmit", 8192, nullptr, 3, nullptr, 0);
    xTaskCreatePinnedToCore(splTickTask,  "splTick",  2048, nullptr, 2, nullptr, 0);
}

void loop() {
    handlePMUInterrupt();
    vTaskDelay(pdMS_TO_TICKS(100));
}
