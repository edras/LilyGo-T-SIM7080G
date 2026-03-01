#pragma once
#include <Arduino.h>

// Returns a stable unique device ID derived from the ESP32 chip MAC,
// e.g. "brac-A1B2C3D4E5F6". Never changes across reboots or reflashes.
String  provGetDeviceID();

// Load the Bearer token from NVS. Returns empty string if not provisioned yet.
String  provLoadToken();

// Save a token received from the server into NVS.
void    provSaveToken(const String &token);

// True if a token exists in NVS.
bool    provIsProvisioned();
