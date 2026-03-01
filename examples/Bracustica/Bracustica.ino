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
#include "provisioning.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>

// ── Event bits ───────────────────────────────────────────────────
#define EVT_THRESHOLD_CROSSED  BIT0
#define EVT_TRANSMIT_SPL       BIT1

static EventGroupHandle_t events;

// ── Runtime-configurable parameters (updated from server responses) ──
static volatile float    gSPLThresholdDB = SPL_THRESHOLD_DB;
static volatile uint32_t gTXIntervalS    = SPL_TRANSMIT_INTERVAL_S;

// ── Shared state (written by audioTask, read by others) ──────────
static float  splCircular[RING_BUFFER_SECONDS] = {};
static int    splHead  = 0;
static int    splCount = 0;
static float  lastSPL  = 0;
static SemaphoreHandle_t splMtx;

// ── Buffers ───────────────────────────────────────────────────────
static int16_t sampleAccum[SAMPLES_PER_SECOND];
static float   filteredBuf[SAMPLES_PER_SECOND];
static uint8_t adpcmSecBuf[ADPCM_BYTES_PER_SEC];
static uint8_t alertBuf[ALERT_BUFFER_SIZE];

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

        applyAWeighting(sampleAccum, filteredBuf, SAMPLES_PER_SECOND);
        float rms = computeRMS(filteredBuf, SAMPLES_PER_SECOND);
        float spl = rmsToDBSPL(rms);

        xSemaphoreTake(splMtx, portMAX_DELAY);
        splCircular[splHead] = spl;
        splHead = (splHead + 1) % RING_BUFFER_SECONDS;
        if (splCount < RING_BUFFER_SECONDS) splCount++;
        lastSPL = spl;
        xSemaphoreGive(splMtx);

        size_t bytes = adpcmEncode(encState, sampleAccum, SAMPLES_PER_SECOND, adpcmSecBuf);
        audioRingPushSecond(adpcmSecBuf, bytes);

        if (spl >= gSPLThresholdDB) {
            xEventGroupSetBits(events, EVT_THRESHOLD_CROSSED);
        }

        GPSData gps = gpsGetCached();
        sdLogSPL(gpsCurrentEpoch(), spl, gps.lat, gps.lon);

        Serial.printf("[Audio] SPL=%.1f dB  RMS=%.5f\n", spl, rms);
    }
}

// ── transmitTask ─────────────────────────────────────────────────
static void transmitTask(void *) {
    static bool     modemReady    = false;
    static uint32_t lastGPSUpdate = 0;

    vTaskDelay(pdMS_TO_TICKS(10000));

    if (!modemInit()) {
        Serial.println("[TX] Modem init failed — transmit disabled");
        vTaskDelete(nullptr);
        return;
    }

    // ── Load or obtain auth token ─────────────────────────────────
    String token = provLoadToken();
    if (token.length() == 0) {
        // Not provisioned yet — start registration flow
        String deviceID = provGetDeviceID();
        Serial.printf("[Prov] Device ID: %s\n", deviceID.c_str());
        Serial.println("[Prov] No token — starting registration");

        bool tokenObtained = false;
        while (!tokenObtained) {
            if (!modemConnect()) { vTaskDelay(pdMS_TO_TICKS(30000)); continue; }
            String newToken;
            RegResult r = modemRegister(deviceID, newToken);
            modemDisconnect();
            if (r == RegResult::OK) {
                provSaveToken(newToken);
                token = newToken;
                tokenObtained = true;
                Serial.println("[Prov] Token received and saved");
            } else if (r == RegResult::PENDING) {
                Serial.println("[Prov] Awaiting admin approval — retrying in 60 s");
                vTaskDelay(pdMS_TO_TICKS(60000));
            } else {
                Serial.println("[Prov] Registration error — retrying in 30 s");
                vTaskDelay(pdMS_TO_TICKS(30000));
            }
        }
    }
    modemSetToken(token);
    Serial.println("[Prov] Auth token loaded");

    if (!modemConnect()) {
        Serial.println("[TX] Initial network connect failed — transmit disabled");
        vTaskDelete(nullptr);
        return;
    }
    modemReady = true;
    modemDisconnect();

    while (true) {
        EventBits_t bits = xEventGroupWaitBits(events,
            EVT_TRANSMIT_SPL | EVT_THRESHOLD_CROSSED,
            pdTRUE, pdFALSE,
            pdMS_TO_TICKS(gTXIntervalS * 1000UL));

        if (!modemReady) continue;

        // ── Audio alert (priority — handle before SPL report) ─────
        if (bits & EVT_THRESHOLD_CROSSED) {
            uint32_t ts       = gpsCurrentEpoch();
            size_t alertBytes = audioRingGetLast(ALERT_SECONDS, alertBuf);
            float  peak       = lastSPL;
            displayAlert(peak);
            sdSaveAlert(ts, alertBuf, alertBytes);
            if (modemConnect()) {
                sendAudioAlert(alertBuf, alertBytes, peak, gpsGetCached(), ts);
                modemDisconnect();
            }
        }

        // ── SPL report + server config update ─────────────────────
        {
            uint32_t ts = gpsCurrentEpoch();
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
                ServerConfig cfg = sendSPLReport(snapshot, n, gpsGetCached(), ts);
                modemDisconnect();

                if (cfg.valid) {
                    // Apply threshold update
                    if (cfg.thresholdDB > 0.0f) {
                        gSPLThresholdDB = cfg.thresholdDB;
                        Serial.printf("[Config] Threshold → %.1f dB\n", cfg.thresholdDB);
                    }
                    // Apply TX interval update
                    if (cfg.intervalS > 0) {
                        gTXIntervalS = cfg.intervalS;
                        Serial.printf("[Config] TX interval → %u s\n", cfg.intervalS);
                    }
                    // GPS refresh + clock re-sync
                    if (cfg.gpsRefresh) {
                        Serial.println("[Config] Server requested GPS refresh");
                        gpsTryFix();
                        lastGPSUpdate = gpsCurrentEpoch();
                    }
                }
            }
        }

        // ── Periodic GPS (fallback when server never requests it) ─
        {
            uint32_t nowEpoch = gpsCurrentEpoch();
            if (lastGPSUpdate == 0 ||
                (nowEpoch > 0 && nowEpoch - lastGPSUpdate >= GPS_INTERVAL_S)) {
                gpsTryFix();
                lastGPSUpdate = gpsCurrentEpoch();
            }
        }

        displayStatus(lastSPL, getBatteryVoltage(), modemReady, true);
    }
}

// ── SPL tick task ─────────────────────────────────────────────────
static void splTickTask(void *) {
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(gTXIntervalS * 1000UL));
        xEventGroupSetBits(events, EVT_TRANSMIT_SPL);
    }
}

// ── setup / loop ─────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    while (!Serial);
    delay(1000);
    Serial.println("[Bracustica] Boot");

    events = xEventGroupCreate();
    splMtx = xSemaphoreCreateMutex();

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
