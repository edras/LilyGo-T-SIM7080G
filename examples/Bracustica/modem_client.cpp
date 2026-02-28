#include "modem_client.h"
#include "config.h"
#include "power_manager.h"

#include <TinyGsmClient.h>

TinyGsm  gsm(Serial1);
static bool     registered = false;

// ── helpers ──────────────────────────────────────────────────────

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

// ── public ───────────────────────────────────────────────────────

bool modemInit() {
    Serial1.begin(115200, SERIAL_8N1, MODEM_RXD, MODEM_TXD);
    modemWake();
    modemPowerOn();

    if (gsm.getSimStatus() != SIM_READY) {
        Serial.println("[Modem] SIM not ready");
        return false;
    }

    // Disable RF while configuring
    gsm.sendAT("+CFUN=0");
    gsm.waitResponse(20000UL);

    gsm.setNetworkMode(2);                  // automatic
    gsm.setPreferredMode(2);                // NB-IoT

    gsm.sendAT("+CGDCONT=1,\"IP\",\"" MODEM_APN "\"");
    gsm.waitResponse();
    gsm.sendAT("+CNCFG=0,1,\"" MODEM_APN "\"");
    gsm.waitResponse();

    // Enable PSM with T3412=30min, T3324=2s active
    gsm.sendAT("+CPSMS=1,,,\"00100011\",\"00000001\"");
    gsm.waitResponse();

    // Enable sleep via DTR
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

// ── HTTP helpers ─────────────────────────────────────────────────

static TinyGsmClient client(gsm, 0);

static bool openTCP() {
    return client.connect(SERVER_HOST, SERVER_PORT);
}

static int readStatusCode() {
    // Parse "HTTP/1.x NNN " from response stream
    uint32_t t = millis();
    while (client.connected() && !client.available() && millis()-t < 10000) delay(50);
    String line = client.readStringUntil('\n');
    int code = 0;
    if (line.startsWith("HTTP/")) {
        int sp = line.indexOf(' ');
        if (sp > 0) code = line.substring(sp+1, sp+4).toInt();
    }
    while (client.available()) client.read(); // drain
    return code;
}

bool sendSPLReport(const float *splArray, int count, const GPSData &gps, uint32_t epochS) {
    // Build JSON body
    String body = "{\"device\":\"" DEVICE_ID "\",\"ts\":" + String(epochS);
    body += ",\"lat\":" + String(gps.valid ? gps.lat : 0.0f, 6);
    body += ",\"lon\":" + String(gps.valid ? gps.lon : 0.0f, 6);
    body += ",\"spl\":[";
    for (int i = 0; i < count; i++) {
        body += String(splArray[i], 2);
        if (i < count-1) body += ',';
    }
    body += "]}";

    if (!openTCP()) { Serial.println("[HTTP] Connect failed"); return false; }
    client.print("POST " API_SPL_PATH " HTTP/1.0\r\n");
    client.print("Host: " SERVER_HOST "\r\n");
    client.print("Content-Type: application/json\r\n");
    client.printf("Content-Length: %d\r\n\r\n", body.length());
    client.print(body);

    int code = readStatusCode();
    client.stop();
    Serial.printf("[HTTP] SPL POST → %d\n", code);
    return (code >= 200 && code < 300);
}

bool sendAudioAlert(const uint8_t *adpcm, size_t bytes, float peakSPL,
                    const GPSData &gps, uint32_t epochS) {
    if (!openTCP()) { Serial.println("[HTTP] Connect failed"); return false; }
    client.print("POST " API_ALERT_PATH " HTTP/1.0\r\n");
    client.print("Host: " SERVER_HOST "\r\n");
    client.print("Content-Type: audio/adpcm\r\n");
    client.printf("X-Device: " DEVICE_ID "\r\n");
    client.printf("X-Timestamp: %u\r\n", epochS);
    client.printf("X-Peak-SPL: %.2f\r\n", peakSPL);
    client.printf("X-Lat: %.6f\r\n", gps.valid ? gps.lat : 0.0f);
    client.printf("X-Lon: %.6f\r\n", gps.valid ? gps.lon : 0.0f);
    client.printf("X-Sample-Rate: %d\r\n", SAMPLE_RATE);
    client.printf("Content-Length: %d\r\n\r\n", (int)bytes);

    const size_t chunk = 256;
    size_t sent = 0;
    while (sent < bytes) {
        size_t n = (bytes - sent < chunk) ? bytes - sent : chunk;
        client.write(adpcm + sent, n);
        sent += n;
    }

    int code = readStatusCode();
    client.stop();
    Serial.printf("[HTTP] Alert POST → %d (%u bytes)\n", code, (unsigned)bytes);
    return (code >= 200 && code < 300);
}
