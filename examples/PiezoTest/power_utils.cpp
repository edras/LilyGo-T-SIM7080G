#include "power_utils.h"

#define XPOWERS_CHIP_AXP2101
#include "XPowersLib.h"
#include "utilities.h"

XPowersPMU PMU;
bool pmu_flag = false;

uint8_t curIndex = 4;

void setFlag(void) {

    pmu_flag = true;
}

void printChargerConstantCurr() {
    const uint16_t currTable[] = {
        0, 0, 0, 0, 100, 125, 150, 175, 200, 300, 400, 500, 600, 700, 800, 900, 1000
    };
    uint8_t val = PMU.getChargerConstantCurr();
    Serial.printf("Setting Charge Target Current - VAL:%u CURRENT:%u\n", val, currTable[val]);
}

void initPMU() {
    bool res = PMU.begin(Wire1, AXP2101_SLAVE_ADDRESS, I2C_SDA, I2C_SCL);
    if (!res) {
        Serial.println("Failed to initialize power.....");
        while (1) delay(5000);
    }
    pinMode(PMU_INPUT_PIN, INPUT);
    attachInterrupt(PMU_INPUT_PIN, setFlag, FALLING);
    PMU.disableIRQ(XPOWERS_AXP2101_ALL_IRQ);
    PMU.clearIrqStatus();
    PMU.enableIRQ(
        XPOWERS_AXP2101_BAT_INSERT_IRQ    | XPOWERS_AXP2101_BAT_REMOVE_IRQ      |
        XPOWERS_AXP2101_VBUS_INSERT_IRQ   | XPOWERS_AXP2101_VBUS_REMOVE_IRQ     |
        XPOWERS_AXP2101_PKEY_SHORT_IRQ    | XPOWERS_AXP2101_PKEY_LONG_IRQ       |
        XPOWERS_AXP2101_BAT_CHG_DONE_IRQ  | XPOWERS_AXP2101_BAT_CHG_START_IRQ
    );
    PMU.setPrechargeCurr(XPOWERS_AXP2101_PRECHARGE_50MA);
    PMU.setChargerConstantCurr(XPOWERS_AXP2101_CHG_CUR_0MA);
    PMU.setChargeTargetVoltage(XPOWERS_AXP2101_CHG_VOL_4V2);
    PMU.disableDC2(); PMU.disableDC3(); PMU.disableDC4(); PMU.disableDC5();
    PMU.disableALDO1(); PMU.disableALDO2(); PMU.disableALDO3(); PMU.disableALDO4();
    PMU.disableBLDO1(); PMU.disableBLDO2();
    PMU.disableCPUSLDO(); PMU.disableDLDO1(); PMU.disableDLDO2();
    PMU.setChargingLedMode(XPOWERS_CHG_LED_BLINK_4HZ);
    printChargerConstantCurr();
}

void handlePMUInterrupt() {
    extern bool pmu_flag;
    if (pmu_flag) {
        pmu_flag = false;
        uint32_t status = PMU.getIrqStatus();
        if (PMU.isPekeyShortPressIrq()) {
            if (!PMU.setChargerConstantCurr(curIndex)) {
                Serial.println("Setting Charger Constant Current Failed!");
            }
            printChargerConstantCurr();
            Serial.println("===========================");
            curIndex++;
            curIndex %= (XPOWERS_AXP2101_CHG_CUR_1000MA + 1);
            PMU.setChargingLedMode(PMU.getChargingLedMode() != XPOWERS_CHG_LED_OFF ? XPOWERS_CHG_LED_OFF : XPOWERS_CHG_LED_ON);
        }
        PMU.clearIrqStatus();
    }
}

void setChargingLedOn()
{
    PMU.setChargingLedMode(XPOWERS_CHG_LED_ON);
}
void setChargingLedOff()
{
    PMU.setChargingLedMode(XPOWERS_CHG_LED_OFF);
}
void setChargingLed4Hz()
{
    PMU.setChargingLedMode(XPOWERS_CHG_LED_BLINK_4HZ);
}
void setChargingLed1Hz()
{
    PMU.setChargingLedMode(XPOWERS_CHG_LED_BLINK_1HZ);
}

