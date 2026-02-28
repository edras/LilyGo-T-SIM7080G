#pragma once
#include <Arduino.h>

bool initSD();
// Append one CSV row: epoch_s, spl_db, lat, lon
void sdLogSPL(uint32_t epochS, float spl, float lat, float lon);
// Save a raw ADPCM alert file named /alert_<epochS>.adpcm
void sdSaveAlert(uint32_t epochS, const uint8_t *adpcm, size_t bytes);
