#include "sd_storage.h"
#include "config.h"
#include <SD_MMC.h>

static bool sdReady = false;

bool initSD() {
    SD_MMC.setPins(SDMMC_CLK_PIN, SDMMC_CMD_PIN, SDMMC_DATA_PIN);
    if (!SD_MMC.begin("/sdcard", true, false, SDMMC_FREQ_DEFAULT)) {
        Serial.println("[SD] Mount failed");
        return false;
    }
    sdReady = true;
    if (!SD_MMC.exists("/spl.csv")) {
        File f = SD_MMC.open("/spl.csv", FILE_WRITE);
        if (f) { f.println("timestamp,spl,lat,lon"); f.close(); }
    }
    return true;
}

void sdLogSPL(uint32_t epochS, float spl, float lat, float lon) {
    if (!sdReady) return;
    File f = SD_MMC.open("/spl.csv", FILE_APPEND);
    if (!f) return;
    f.printf("%u,%.2f,%.6f,%.6f\n", epochS, spl, lat, lon);
    f.close();
}

void sdSaveAlert(uint32_t epochS, const uint8_t *adpcm, size_t bytes) {
    if (!sdReady) return;
    char path[32];
    snprintf(path, sizeof(path), "/alert_%u.adpcm", epochS);
    File f = SD_MMC.open(path, FILE_WRITE);
    if (!f) return;
    const size_t chunk = 512;
    size_t written = 0;
    while (written < bytes) {
        size_t n = (bytes - written < chunk) ? bytes - written : chunk;
        f.write(adpcm + written, n);
        written += n;
    }
    f.close();
    Serial.printf("[SD] Saved %s (%u bytes)\n", path, (unsigned)bytes);
}
