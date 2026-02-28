#pragma once
#include "modem_client.h"   // GPSData

void gpsManagerInit();
// Attempt to get a fix. Enables GPS antenna power, waits up to GPS_FIX_TIMEOUT_S.
// Updates the cached GPSData and returns true on success.
bool gpsTryFix();
// Returns last cached fix (may have valid=false if no fix yet).
GPSData gpsGetCached();
