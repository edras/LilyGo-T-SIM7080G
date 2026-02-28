#pragma once

// ── I2S MEMS Microphone ─────────────────────────────────────────
#define I2S_BCK_PIN         14
#define I2S_WS_PIN          13
#define I2S_DATA_IN_PIN     11

// ── OLED Display (Wire, 128×64 SSD1306) ─────────────────────────
#define DISPLAY_SDA         17
#define DISPLAY_SCL         16
#define DISPLAY_ADDR        0x3C
#define ENABLE_DISPLAY

// ── PMU AXP2101 (Wire1) ─────────────────────────────────────────
#define PMU_SDA             15
#define PMU_SCL             7
#define PMU_IRQ_PIN         6

// ── SIM7080G Modem ───────────────────────────────────────────────
#define MODEM_RXD           4
#define MODEM_TXD           5
#define MODEM_PWR_PIN       41
#define MODEM_DTR_PIN       42
#define MODEM_RI_PIN        3

// ── SD Card (SDMMC 1-bit) ───────────────────────────────────────
#define SDMMC_CMD_PIN       39
#define SDMMC_CLK_PIN       38
#define SDMMC_DATA_PIN      40

// ── User Button ─────────────────────────────────────────────────
#define USER_BUTTON_PIN     0

// ── Audio ────────────────────────────────────────────────────────
#define SAMPLE_RATE             8000
#define SAMPLES_PER_SECOND      8000
#define I2S_DMA_BUF_COUNT       8
#define I2S_DMA_BUF_LEN        1024
#define ADPCM_BYTES_PER_SEC    4000   // 8000 samples × 0.5 bytes (4-bit ADPCM)

// ── Ring Buffer ──────────────────────────────────────────────────
#define RING_BUFFER_SECONDS     60
#define ALERT_SECONDS           10
#define RING_BUFFER_SIZE        (RING_BUFFER_SECONDS * ADPCM_BYTES_PER_SEC)  // 240 KB
#define ALERT_BUFFER_SIZE       (ALERT_SECONDS * ADPCM_BYTES_PER_SEC)        // 40 KB

// ── SPL ──────────────────────────────────────────────────────────
#define SPL_THRESHOLD_DB        85.0f
#define SPL_REF_PA              0.00002f  // 20 µPa reference

// ── Timing ───────────────────────────────────────────────────────
#define SPL_TRANSMIT_INTERVAL_S 60
#define GPS_INTERVAL_S          1800     // 30 minutes
#define GPS_FIX_TIMEOUT_S       120

// ── NB-IoT ───────────────────────────────────────────────────────
#define MODEM_APN               "TM"
#define SERVER_HOST             "your.server.com"
#define SERVER_PORT             80
#define API_SPL_PATH            "/api/v1/spl"
#define API_ALERT_PATH          "/api/v1/alert"
#define DEVICE_ID               "bracustica-001"

// ── Power ────────────────────────────────────────────────────────
#define CPU_MAX_FREQ_MHZ        80
#define CPU_MIN_FREQ_MHZ        10
