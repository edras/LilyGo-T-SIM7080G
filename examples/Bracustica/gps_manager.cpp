#include "gps_manager.h"
#include "config.h"
#include "power_manager.h"

#include <TinyGsmClient.h>
#include <time.h>

extern TinyGsm gsm;   // defined in modem_client.cpp — same modem object

static GPSData  cachedFix       = {0, 0, 0, false};
static time_t   epochAtFix      = 0;   // Unix epoch captured at last GPS fix
static uint32_t millisAtFix     = 0;   // millis() at the moment of that fix

void gpsManagerInit() {}

// Convert GPS date/time fields (UTC) to Unix epoch.
// mktime() interprets struct tm as local time; on ESP32 without TZ set the
// default offset is 0 (UTC), so this is correct. setenv("TZ","UTC0",1) makes
// it explicit and safe regardless of any future TZ configuration.
static time_t gpsToEpoch(int yr, int mo, int dy, int hr, int mn, int sc) {
    setenv("TZ", "UTC0", 1);
    tzset();
    struct tm t = {};
    t.tm_year  = yr - 1900;
    t.tm_mon   = mo - 1;
    t.tm_mday  = dy;
    t.tm_hour  = hr;
    t.tm_min   = mn;
    t.tm_sec   = sc;
    t.tm_isdst = 0;
    return mktime(&t);
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
        epochAtFix   = gpsToEpoch(yr, mo, dy, hr, mn, sc);
        millisAtFix  = millis();
        cachedFix.lat    = lat;
        cachedFix.lon    = lon;
        cachedFix.epochS = (uint32_t)epochAtFix;
        cachedFix.valid  = true;
        Serial.printf("[GPS] Fix: %.6f, %.6f  epoch=%u\n", lat, lon, (uint32_t)epochAtFix);
    } else {
        Serial.println("[GPS] No fix within timeout");
    }
    return got;
}

GPSData gpsGetCached() {
    return cachedFix;
}

uint32_t gpsCurrentEpoch() {
    if (epochAtFix == 0) return 0;
    // millis() is monotonic (wraps after ~49 days, handled by unsigned subtraction)
    uint32_t elapsedS = (millis() - millisAtFix) / 1000UL;
    return (uint32_t)epochAtFix + elapsedS;
}
