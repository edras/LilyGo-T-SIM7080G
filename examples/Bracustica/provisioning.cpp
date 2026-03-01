#include "provisioning.h"
#include "config.h"
#include <Preferences.h>
#include <esp_efuse.h>
#include <esp_mac.h>

static Preferences prefs;

String provGetDeviceID() {
    uint8_t mac[6];
    esp_efuse_mac_get_default(mac);
    char id[20];
    snprintf(id, sizeof(id), "brac-%02X%02X%02X%02X%02X%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return String(id);
}

String provLoadToken() {
    prefs.begin(NVS_NAMESPACE, true);  // read-only
    String token = prefs.getString(NVS_TOKEN_KEY, "");
    prefs.end();
    return token;
}

void provSaveToken(const String &token) {
    prefs.begin(NVS_NAMESPACE, false);
    prefs.putString(NVS_TOKEN_KEY, token);
    prefs.end();
    Serial.println("[Prov] Token saved to NVS");
}

bool provIsProvisioned() {
    return provLoadToken().length() > 0;
}
