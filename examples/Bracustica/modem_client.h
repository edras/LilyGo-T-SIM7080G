#pragma once
#include <Arduino.h>

struct GPSData {
    float    lat;
    float    lon;
    uint32_t epochS;
    bool     valid;
};

bool modemInit();
bool modemConnect();          // Register to NB-IoT, activate bearer
void modemDisconnect();

// POST last 60 SPL floats + GPS as JSON. Returns true on HTTP 2xx.
bool sendSPLReport(const float *splArray, int count, const GPSData &gps, uint32_t epochS);

// POST compressed ADPCM alert audio. Returns true on HTTP 2xx.
bool sendAudioAlert(const uint8_t *adpcm, size_t bytes, float peakSPL,
                    const GPSData &gps, uint32_t epochS);
