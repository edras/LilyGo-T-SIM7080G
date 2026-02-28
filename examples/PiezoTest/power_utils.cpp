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

    /*********************************
     *  step 1 : Initialize power chip,
     *  turn on modem and gps antenna power channel
    ***********************************/
    bool res = PMU.begin(Wire1, AXP2101_SLAVE_ADDRESS, I2C_SDA, I2C_SCL);
    if (!res) {
        Serial.println("Failed to initialize power.....");
        while (1) delay(5000);
    }

    // If it is a power cycle, turn off the modem power. Then restart it
    if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_UNDEFINED ) {
        PMU.disableDC3();
        // Wait a minute
        delay(200);
    }

    // Set the minimum common working voltage of the PMU VBUS input,
    // below this value will turn off the PMU
    PMU.setVbusVoltageLimit(XPOWERS_AXP2101_VBUS_VOL_LIM_4V36);

        // Set the maximum current of the PMU VBUS input,
    // higher than this value will turn off the PMU
    PMU.setVbusCurrentLimit(XPOWERS_AXP2101_VBUS_CUR_LIM_1500MA);

    // Set VSY off voltage as 2600mV , Adjustment range 2600mV ~ 3300mV
    PMU.setSysPowerDownVoltage(2600);

    // TS Pin detection must be disable, otherwise it cannot be charged
    PMU.disableTSPinMeasure();

    /*********************************
     * step 2 : Enable internal ADC detection
    ***********************************/
    PMU.enableBattDetection();
    PMU.enableVbusVoltageMeasure();
    PMU.enableBattVoltageMeasure();
    PMU.enableSystemVoltageMeasure();

    /*********************************
     * step 3 : Set PMU interrupt
    ***********************************/
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

    /*********************************
     * step 4 : Set PMU Charger params
    ***********************************/
    PMU.setPrechargeCurr(XPOWERS_AXP2101_PRECHARGE_50MA);
    PMU.setChargerConstantCurr(XPOWERS_AXP2101_CHG_CUR_0MA);
    PMU.setChargeTargetVoltage(XPOWERS_AXP2101_CHG_VOL_4V2);

    /*********************************
     * step 5 : Disable all power channel 
    ***********************************/
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

void printPMUStatus()
{
    Serial.printf("PMU Status: 0x%04X\n", PMU.status());
    Serial.printf("PMU Chip ID: 0x%02X\n", PMU.getChipID());
    Serial.printf("PMU isCharging: %s\n", PMU.isCharging() ? "true" : "false");
    Serial.printf("PMU isDischarge: %s\n", PMU.isDischarge() ? "true" : "false");
    Serial.printf("PMU isVbusIn: %s\n", PMU.isVbusIn() ? "true" : "false");
}
void printPMUVoltage()
{
    Serial.println("DCDC=======================================================================");
    Serial.printf("DC1  : %s   Voltage:%u mV \n",  PMU.isEnableDC1()  ? "+" : "-", PMU.getDC1Voltage());
    Serial.printf("DC2  : %s   Voltage:%u mV \n",  PMU.isEnableDC2()  ? "+" : "-", PMU.getDC2Voltage());
    Serial.printf("DC3  : %s   Voltage:%u mV \n",  PMU.isEnableDC3()  ? "+" : "-", PMU.getDC3Voltage());
    Serial.printf("DC4  : %s   Voltage:%u mV \n",  PMU.isEnableDC4()  ? "+" : "-", PMU.getDC4Voltage());
    Serial.printf("DC5  : %s   Voltage:%u mV \n",  PMU.isEnableDC5()  ? "+" : "-", PMU.getDC5Voltage());
    Serial.println("ALDO=======================================================================");
    Serial.printf("ALDO1: %s   Voltage:%u mV\n",  PMU.isEnableALDO1()  ? "+" : "-", PMU.getALDO1Voltage());
    Serial.printf("ALDO2: %s   Voltage:%u mV\n",  PMU.isEnableALDO2()  ? "+" : "-", PMU.getALDO2Voltage());
    Serial.printf("ALDO3: %s   Voltage:%u mV\n",  PMU.isEnableALDO3()  ? "+" : "-", PMU.getALDO3Voltage());
    Serial.printf("ALDO4: %s   Voltage:%u mV\n",  PMU.isEnableALDO4()  ? "+" : "-", PMU.getALDO4Voltage());
    Serial.println("BLDO=======================================================================");
    Serial.printf("BLDO1: %s   Voltage:%u mV\n",  PMU.isEnableBLDO1()  ? "+" : "-", PMU.getBLDO1Voltage());
    Serial.printf("BLDO2: %s   Voltage:%u mV\n",  PMU.isEnableBLDO2()  ? "+" : "-", PMU.getBLDO2Voltage());
    Serial.println("CPUSLDO====================================================================");
    Serial.printf("CPUSLDO: %s Voltage:%u mV\n",  PMU.isEnableCPUSLDO() ? "+" : "-", PMU.getCPUSLDOVoltage());
    Serial.println("DLDO=======================================================================");
    Serial.printf("DLDO1: %s   Voltage:%u mV\n",  PMU.isEnableDLDO1()  ? "+" : "-", PMU.getDLDO1Voltage());
    Serial.printf("DLDO2: %s   Voltage:%u mV\n",  PMU.isEnableDLDO2()  ? "+" : "-", PMU.getDLDO2Voltage());
    Serial.println("===========================================================================");

}


