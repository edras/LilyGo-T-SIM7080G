#pragma once
#include <Arduino.h>

struct GPSData {
    float    lat;
    float    lon;
    uint32_t epochS;
    bool     valid;
};

// Config delivered by the server in the SPL report response body.
struct ServerConfig {
    bool     valid;           // true if HTTP 2xx received and body parsed
    bool     gpsRefresh;      // server requests a fresh GPS fix + clock re-sync
    float    thresholdDB;     // new SPL alert threshold; 0 = no change
    uint32_t intervalS;       // next TX interval in seconds; 0 = no change
};

// Registration result
enum class RegResult { OK, PENDING, ERROR };

bool      modemInit();
bool      modemConnect();
void      modemDisconnect();

// Set the Bearer token to use for all API calls (call after loading from NVS).
void      modemSetToken(const String &token);

// POST device registration request. Returns OK (token issued), PENDING
// (admin has not approved yet), or ERROR. On OK, `tokenOut` is filled.
RegResult modemRegister(const String &deviceID, String &tokenOut);

// POST SPL array as JSON. Returns server config parsed from response body.
ServerConfig sendSPLReport(const float *splArray, int count,
                           const GPSData &gps, uint32_t epochS);

// POST compressed ADPCM alert audio. Returns true on HTTP 2xx.
bool sendAudioAlert(const uint8_t *adpcm, size_t bytes, float peakSPL,
                    const GPSData &gps, uint32_t epochS);
