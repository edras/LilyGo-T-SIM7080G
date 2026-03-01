#pragma once
#include "modem_client.h"   // GPSData

void     gpsManagerInit();
bool     gpsTryFix();
GPSData  gpsGetCached();
// Current Unix epoch estimated from last GPS fix + elapsed millis().
// Returns 0 if no fix has ever been obtained.
uint32_t gpsCurrentEpoch();
