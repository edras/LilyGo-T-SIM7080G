#include "gps_manager.h"
#include "config.h"
#include "power_manager.h"

#include <TinyGsmClient.h>

extern TinyGsm gsm;   // defined in modem_client.cpp — same modem object

static GPSData cachedFix = {0, 0, 0, false};

void gpsManagerInit() {
    // Nothing to initialise — GPS uses the shared modem via AT commands.
}

bool gpsTryFix() {
    enableGPSPower();

    if (!gsm.enableGPS()) {
        Serial.println("[GPS] Enable failed");
        disableGPSPower();
        return false;
    }

    float lat = 0, lon = 0, spd = 0, alt = 0, acc = 0;
    int   vsat = 0, usat = 0;
    int   yr = 0, mo = 0, dy = 0, hr = 0, mn = 0, sc = 0;

    uint32_t deadline = millis() + (uint32_t)GPS_FIX_TIMEOUT_S * 1000UL;
    bool got = false;
    while (millis() < deadline) {
        if (gsm.getGPS(&lat, &lon, &spd, &alt, &vsat, &usat, &acc,
                        &yr, &mo, &dy, &hr, &mn, &sc)) {
            got = true;
            break;
        }
        delay(2000);
    }

    gsm.disableGPS();
    disableGPSPower();

    if (got) {
        cachedFix.lat    = lat;
        cachedFix.lon    = lon;
        cachedFix.epochS = (uint32_t)millis() / 1000; // placeholder until RTC available
        cachedFix.valid  = true;
        Serial.printf("[GPS] Fix: %.6f, %.6f\n", lat, lon);
    } else {
        Serial.println("[GPS] No fix within timeout");
    }
    return got;
}

GPSData gpsGetCached() {
    return cachedFix;
}
