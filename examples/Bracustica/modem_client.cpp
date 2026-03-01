#include "modem_client.h"
#include "config.h"
#include "power_manager.h"

#include <TinyGsmClient.h>

TinyGsm            gsm(Serial1);
static TinyGsmClientSecure client(gsm, 0);
static bool        registered = false;
static String      bearerToken;

// ── modem init / connect ─────────────────────────────────────────

static void modemPowerOn() {
    pinMode(MODEM_PWR_PIN, OUTPUT);
    pinMode(MODEM_DTR_PIN, OUTPUT);
    pinMode(MODEM_RI_PIN, INPUT);
    enableModemPower();
    delay(100);

    int retry = 0;
    while (!gsm.testAT(1000)) {
        if (retry++ > 6) {
            digitalWrite(MODEM_PWR_PIN, LOW); delay(100);
            digitalWrite(MODEM_PWR_PIN, HIGH); delay(1000);
            digitalWrite(MODEM_PWR_PIN, LOW);
            retry = 0;
            Serial.println("[Modem] Retry power cycle");
        }
    }
    Serial.println("[Modem] AT OK");
}

bool modemInit() {
    Serial1.begin(115200, SERIAL_8N1, MODEM_RXD, MODEM_TXD);
    modemWake();
    modemPowerOn();

    if (gsm.getSimStatus() != SIM_READY) {
        Serial.println("[Modem] SIM not ready");
        return false;
    }

    gsm.sendAT("+CFUN=0");
    gsm.waitResponse(20000UL);

    gsm.setNetworkMode(2);
    gsm.setPreferredMode(2);   // NB-IoT

    gsm.sendAT("+CGDCONT=1,\"IP\",\"" MODEM_APN "\"");
    gsm.waitResponse();
    gsm.sendAT("+CNCFG=0,1,\"" MODEM_APN "\"");
    gsm.waitResponse();

    // SSL context 0: TLS 1.2, no server cert verification.
    // Change sslversion to 4 (TLS 1.2) and authmode to 1 to verify server cert
    // (requires uploading the Let's Encrypt ISRG Root X1 PEM via AT+CFSWFILE).
    gsm.sendAT("+CSSLCFG=\"sslversion\",0,3");   // 3 = TLS 1.2
    gsm.waitResponse();
    gsm.sendAT("+CSSLCFG=\"authmode\",0,0");     // 0 = no server cert check
    gsm.waitResponse();
    gsm.sendAT("+CSSLCFG=\"ignorertctime\",0,1");
    gsm.waitResponse();

    // PSM: T3412=30 min, T3324=2 s active
    gsm.sendAT("+CPSMS=1,,,\"00100011\",\"00000001\"");
    gsm.waitResponse();

    gsm.sendAT("+CSCLK=1");
    gsm.waitResponse();

    gsm.sendAT("+CFUN=1");
    gsm.waitResponse(20000UL);

    return true;
}

bool modemConnect() {
    modemWake();
    SIM70xxRegStatus s;
    uint32_t start = millis();
    do {
        s = gsm.getRegistrationStatus();
        if (millis() - start > 60000UL) {
            Serial.println("[Modem] Registration timeout");
            return false;
        }
        delay(1000);
    } while (s != REG_OK_HOME && s != REG_OK_ROAMING);

    gsm.sendAT("+CNACT=0,1");
    if (gsm.waitResponse(10000) != 1) {
        Serial.println("[Modem] Bearer activation failed");
        return false;
    }
    registered = true;
    return true;
}

void modemDisconnect() {
    gsm.sendAT("+CNACT=0,0");
    gsm.waitResponse(5000);
    modemSleep();
    registered = false;
}

void modemSetToken(const String &token) {
    bearerToken = token;
}

// ── HTTP helpers ─────────────────────────────────────────────────

static bool openTLS() {
    return client.connect(SERVER_HOST, SERVER_PORT);
}

// Writes common request headers including Authorization if a token is set.
static void writeHeaders(const char *method, const char *path,
                         const char *contentType, size_t bodyLen) {
    client.printf("%s %s HTTP/1.0\r\n", method, path);
    client.printf("Host: %s\r\n", SERVER_HOST);
    if (contentType) client.printf("Content-Type: %s\r\n", contentType);
    if (bodyLen > 0) client.printf("Content-Length: %d\r\n", (int)bodyLen);
    if (bearerToken.length() > 0)
        client.printf("Authorization: Bearer %s\r\n", bearerToken.c_str());
    client.print("\r\n");
}

static int readHttpResponse(String &body) {
    uint32_t t = millis();
    while (client.connected() && !client.available() && millis() - t < 10000) delay(50);

    String statusLine = client.readStringUntil('\n');
    int code = 0;
    if (statusLine.startsWith("HTTP/")) {
        int sp = statusLine.indexOf(' ');
        if (sp > 0) code = statusLine.substring(sp + 1, sp + 4).toInt();
    }

    while (client.connected() || client.available()) {
        String line = client.readStringUntil('\n');
        line.trim();
        if (line.length() == 0) break;
    }

    body = "";
    uint32_t deadline = millis() + 5000;
    while ((client.connected() || client.available()) && millis() < deadline) {
        while (client.available()) {
            char c = client.read();
            if (body.length() < 512) body += c;
        }
        delay(10);
    }
    return code;
}

static float jsonFloat(const String &json, const char *key, float fallback) {
    String search = "\""; search += key; search += "\":";
    int idx = json.indexOf(search);
    if (idx < 0) return fallback;
    return json.substring(idx + search.length()).toFloat();
}

static bool jsonBool(const String &json, const char *key, bool fallback) {
    String search = "\""; search += key; search += "\":";
    int idx = json.indexOf(search);
    if (idx < 0) return fallback;
    return json.substring(idx + search.length(), idx + search.length() + 5).equalsIgnoreCase("true");
}

// Extract a quoted string value: "key":"value"
static String jsonString(const String &json, const char *key) {
    String search = "\""; search += key; search += "\":\"";
    int start = json.indexOf(search);
    if (start < 0) return "";
    start += search.length();
    int end = json.indexOf('"', start);
    if (end < 0) return "";
    return json.substring(start, end);
}

// ── API calls ────────────────────────────────────────────────────

RegResult modemRegister(const String &deviceID, String &tokenOut) {
    String body = "{\"device_id\":\"" + deviceID + "\",\"fw\":\"1.0\"}";

    if (!openTLS()) { Serial.println("[Reg] TLS connect failed"); return RegResult::ERROR; }
    writeHeaders("POST", API_REGISTER_PATH, "application/json", body.length());
    client.print(body);

    String respBody;
    int code = readHttpResponse(respBody);
    client.stop();
    Serial.printf("[Reg] %d  %s\n", code, respBody.c_str());

    if (code == 200) {
        tokenOut = jsonString(respBody, "token");
        return tokenOut.length() > 0 ? RegResult::OK : RegResult::ERROR;
    }
    if (code == 202) return RegResult::PENDING;   // admin has not approved yet
    return RegResult::ERROR;
}

ServerConfig sendSPLReport(const float *splArray, int count,
                           const GPSData &gps, uint32_t epochS) {
    ServerConfig cfg = {false, false, 0.0f, 0};

    String body = "{\"device_id\":\"" + String(SERVER_HOST) + "\",\"ts\":" + String(epochS);
    // Note: device_id is embedded by the server via the token — we still send
    // ts, gps, and spl payload.
    body = "{\"ts\":" + String(epochS);
    body += ",\"lat\":" + String(gps.valid ? gps.lat : 0.0f, 6);
    body += ",\"lon\":" + String(gps.valid ? gps.lon : 0.0f, 6);
    body += ",\"spl\":[";
    for (int i = 0; i < count; i++) {
        body += String(splArray[i], 2);
        if (i < count - 1) body += ',';
    }
    body += "]}";

    if (!openTLS()) { Serial.println("[HTTP] TLS connect failed"); return cfg; }
    writeHeaders("POST", API_SPL_PATH, "application/json", body.length());
    client.print(body);

    String respBody;
    int code = readHttpResponse(respBody);
    client.stop();
    Serial.printf("[HTTP] SPL POST → %d  %s\n", code, respBody.c_str());

    if (code == 401) {
        Serial.println("[HTTP] 401 Unauthorized — token invalid or revoked");
        return cfg;
    }
    if (code < 200 || code >= 300) return cfg;

    cfg.valid       = true;
    cfg.gpsRefresh  = jsonBool (respBody, "gps_refresh",  false);
    cfg.thresholdDB = jsonFloat(respBody, "threshold_db", 0.0f);
    cfg.intervalS   = (uint32_t)jsonFloat(respBody, "interval_s", 0.0f);
    return cfg;
}

bool sendAudioAlert(const uint8_t *adpcm, size_t bytes, float peakSPL,
                    const GPSData &gps, uint32_t epochS) {
    if (!openTLS()) { Serial.println("[HTTP] TLS connect failed"); return false; }

    // Extra metadata as custom headers; Authorization added by writeHeaders.
    client.printf("POST %s HTTP/1.0\r\n", API_ALERT_PATH);
    client.printf("Host: %s\r\n", SERVER_HOST);
    client.printf("Content-Type: audio/adpcm\r\n");
    client.printf("Content-Length: %d\r\n", (int)bytes);
    client.printf("X-Timestamp: %u\r\n", epochS);
    client.printf("X-Peak-SPL: %.2f\r\n", peakSPL);
    client.printf("X-Lat: %.6f\r\n", gps.valid ? gps.lat : 0.0f);
    client.printf("X-Lon: %.6f\r\n", gps.valid ? gps.lon : 0.0f);
    client.printf("X-Sample-Rate: %d\r\n", SAMPLE_RATE);
    if (bearerToken.length() > 0)
        client.printf("Authorization: Bearer %s\r\n", bearerToken.c_str());
    client.print("\r\n");

    const size_t chunk = 256;
    size_t sent = 0;
    while (sent < bytes) {
        size_t n = (bytes - sent < chunk) ? bytes - sent : chunk;
        client.write(adpcm + sent, n);
        sent += n;
    }

    String respBody;
    int code = readHttpResponse(respBody);
    client.stop();
    Serial.printf("[HTTP] Alert POST → %d (%u bytes)\n", code, (unsigned)bytes);
    return (code >= 200 && code < 300);
}
