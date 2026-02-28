#include "power_manager.h"
#include "config.h"
#define XPOWERS_CHIP_AXP2101
#include "XPowersLib.h"
#include <esp_pm.h>
#include <driver/gpio.h>

XPowersPMU PMU;
static volatile bool pmuFlag = false;

static void IRAM_ATTR pmyISR() { pmuFlag = true; }

void initPMU() {
    bool ok = PMU.begin(Wire1, AXP2101_SLAVE_ADDRESS, PMU_SDA, PMU_SCL);
    if (!ok) {
        Serial.println("[PMU] Init failed");
        while (1) delay(5000);
    }

    PMU.setVbusVoltageLimit(XPOWERS_AXP2101_VBUS_VOL_LIM_4V36);
    PMU.setVbusCurrentLimit(XPOWERS_AXP2101_VBUS_CUR_LIM_1500MA);
    PMU.setSysPowerDownVoltage(2600);
    PMU.disableTSPinMeasure();

    PMU.enableBattDetection();
    PMU.enableVbusVoltageMeasure();
    PMU.enableBattVoltageMeasure();
    PMU.enableSystemVoltageMeasure();

    // Disable all channels first, then selectively enable
    PMU.disableDC2(); PMU.disableDC3(); PMU.disableDC4(); PMU.disableDC5();
    PMU.disableALDO1(); PMU.disableALDO2(); PMU.disableALDO3(); PMU.disableALDO4();
    PMU.disableBLDO1(); PMU.disableBLDO2();
    PMU.disableCPUSLDO(); PMU.disableDLDO1(); PMU.disableDLDO2();

    // BLDO1 = level converter for modem serial — always on
    PMU.setBLDO1Voltage(3300);
    PMU.enableBLDO1();

    PMU.setPrechargeCurr(XPOWERS_AXP2101_PRECHARGE_50MA);
    PMU.setChargerConstantCurr(XPOWERS_AXP2101_CHG_CUR_500MA);
    PMU.setChargeTargetVoltage(XPOWERS_AXP2101_CHG_VOL_4V2);

    pinMode(PMU_IRQ_PIN, INPUT);
    attachInterrupt(PMU_IRQ_PIN, pmyISR, FALLING);
    PMU.disableIRQ(XPOWERS_AXP2101_ALL_IRQ);
    PMU.clearIrqStatus();
    PMU.enableIRQ(
        XPOWERS_AXP2101_BAT_INSERT_IRQ | XPOWERS_AXP2101_BAT_REMOVE_IRQ |
        XPOWERS_AXP2101_VBUS_INSERT_IRQ | XPOWERS_AXP2101_VBUS_REMOVE_IRQ |
        XPOWERS_AXP2101_PKEY_SHORT_IRQ | XPOWERS_AXP2101_PKEY_LONG_IRQ
    );
    PMU.setChargingLedMode(XPOWERS_CHG_LED_BLINK_1HZ);
}

void enableLightSleep() {
    esp_pm_config_esp32s3_t cfg = {
        .max_freq_mhz       = CPU_MAX_FREQ_MHZ,
        .min_freq_mhz       = CPU_MIN_FREQ_MHZ,
        .light_sleep_enable = true,
    };
    esp_pm_configure(&cfg);
}

void enableModemPower() {
    PMU.setDC3Voltage(3000);
    PMU.enableDC3();
}

void disableModemPower() {
    PMU.disableDC3();
}

void enableGPSPower() {
    PMU.setBLDO2Voltage(3300);
    PMU.enableBLDO2();
}

void disableGPSPower() {
    PMU.disableBLDO2();
}

void enableSDPower() {
    PMU.setALDO3Voltage(3300);
    PMU.enableALDO3();
}

void disableSDPower() {
    PMU.disableALDO3();
}

void modemSleep() {
    // DTR HIGH → modem enters AT+CSCLK=1 sleep
    digitalWrite(MODEM_DTR_PIN, HIGH);
    gpio_hold_en((gpio_num_t)MODEM_DTR_PIN);
}

void modemWake() {
    gpio_hold_dis((gpio_num_t)MODEM_DTR_PIN);
    gpio_reset_pin((gpio_num_t)MODEM_DTR_PIN);
    digitalWrite(MODEM_DTR_PIN, LOW);
    delay(50);
}

void handlePMUInterrupt() {
    if (!pmuFlag) return;
    pmuFlag = false;
    PMU.getIrqStatus();
    if (PMU.isPekeyShortPressIrq()) {
        PMU.setChargingLedMode(
            PMU.getChargingLedMode() != XPOWERS_CHG_LED_OFF
                ? XPOWERS_CHG_LED_OFF : XPOWERS_CHG_LED_ON);
    }
    if (PMU.isPekeyLongPressIrq()) {
        Serial.println("[PMU] Long press — power down");
    }
    PMU.clearIrqStatus();
}

float getBatteryVoltage() {
    return PMU.getBattVoltage() / 1000.0f;
}
