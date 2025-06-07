/**
 * @file      PiezoTest.ino
 * @author    Lewis He (lewishe@outlook.com)
 * @license   MIT
 * @copyright Copyright (c) 2024  Shenzhen Xin Yuan Electronic Technology Co., Ltd
 * @date      2024-03-29
 * @note      For the inability to set the charging current, please check this post https://github.com/Xinyuan-LilyGO/LilyGo-T-SIM7080G/issues/49
 * For setting a current greater than 500mA, the VBUS power supply must be sufficient. If the input is lower than 5V, the charging current will be below 500mA.
 */
#include <Arduino.h>
#define XPOWERS_CHIP_AXP2101
#include "XPowersLib.h"
#include "utilities.h"

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <driver/i2s.h>

XPowersPMU  PMU;

// I²C Pins for SSD1306 OLED

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1  // No reset pin
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

bool  pmu_flag = false;
uint32_t loopMillis;

#define CALIBRATION_DURATION_MS 1000
#define SAMPLE_RATE 44100
#define BUFFER_SIZE 1024

// Global buffers to avoid stack overflow
int16_t buffer[BUFFER_SIZE];
float filtered[BUFFER_SIZE];

// Coeficientes calculados para 44.1kHz, ordem 2 (biquad) - aproximação
struct Biquad {
    float a0, a1, a2, b1, b2;
    float z1, z2;
};

// Melody and notes
int melody[] = {
  262, 294, 330, 349, 392, 440, 494, 523  // C4, D4, E4, F4, G4, A4, B4, C5
};

int noteDurations[] = {
  4, 4, 4, 4, 4, 4, 4, 4  // Quarter notes for each note
};

#define PIEZO_PIN  47

// I²S Configuration
#define I2S_NUM I2S_NUM_0


void setFlag(void)
{
    pmu_flag = true;
}

void printChargerConstantCurr()
{
    const uint16_t currTable[] = {
        0, 0, 0, 0, 100, 125, 150, 175, 200, 300, 400, 500, 600, 700, 800, 900, 1000
    };
    uint8_t val = PMU.getChargerConstantCurr();
    Serial.printf("Setting Charge Target Current - VAL:%u CURRENT:%u\n", val, currTable[val]);
}

#define PIEZO_SPL_DB 80.0 // SPL do piezo em dB SPL durante a calibração (ajuste conforme seu piezo)


Biquad aweight_biquad = {
    1.0f, -2.0f, 1.0f, // Numerador (a0, a1, a2)
    -1.99004745483398f, 0.99007225036621f, // Denominador (b1, b2)
    0, 0 // Estado
};

// Aplica o filtro A-weighting em um sample
float applyAWeighting(Biquad &filt, float sample) {
    float result = filt.a0 * sample + filt.z1;
    filt.z1 = filt.a1 * sample - filt.b1 * result + filt.z2;
    filt.z2 = filt.a2 * sample - filt.b2 * result;
    return result;
}

// Aplica o filtro A-weighting em todo o buffer
void filterAWeighting(int16_t *in, float *out, int length) {
    // Reset filtro
    aweight_biquad.z1 = aweight_biquad.z2 = 0;
    for (int i = 0; i < length; i++) {
        // Normaliza para float (-1.0 a 1.0)
        float sample = in[i] / 32768.0f;
        out[i] = applyAWeighting(aweight_biquad, sample);
    }
}

// Calcula RMS de um buffer float
float calculateRMSf(float *buffer, int length) {
    double sum = 0;
    for (int i = 0; i < length; i++) {
        sum += buffer[i] * buffer[i];
    }
    return sqrt(sum / (float)length);
}

float calibrationReference = 1.0;  // Default, will be set during calibration
bool isCalibrated = false;

void setup()
{

    Serial.begin(115200);

    // Start while waiting for Serial monitoring
    while (!Serial);

    delay(3000);

    Serial.println();

    /*********************************
     *  step 1 : Initialize power chip,
     *  turn on modem and gps antenna power channel
    ***********************************/
    bool res;
    // Use Wire1
    res = PMU.begin(Wire1, AXP2101_SLAVE_ADDRESS, I2C_SDA, I2C_SCL);
    if (!res) {
        Serial.println("Failed to initialize power.....");
        while (1) {
            delay(5000);
        }
    }

    // If it is a power cycle, turn off the modem power. Then restart it
    if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_UNDEFINED ) {
        PMU.disableDC3();
        // Wait a minute
        delay(200);
    }

    // I2C sensor call example
    int sda = 17;  // You can also use other IO ports
    int scl = 16;  // You can also use other IO ports
    Wire.begin(sda, scl);

    //**\

    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {  // Default I2C address for SSD1306
        Serial.println(F("SSD1306 allocation failed"));
        for (;;);
    }

    display.clearDisplay();
    display.setTextSize(1);  // Normal 1:1 pixel scale
    display.setTextColor(SSD1306_WHITE);  // Draw white text
    display.setCursor(0, 0);
    display.println(F("Initializing..."));
    display.display();

    //**\

    i2s_pin_config_t pin_config = {
        .bck_io_num = 14,    // Bit Clock (SCK) pin
        .ws_io_num = 36,     // Word Select (LRCLK) pin
        .data_out_num = I2S_PIN_NO_CHANGE,  // No Data Out pin (not used for microphones)
        .data_in_num = 11,   // Data In (SD) pin
    };

        // Configure I²S
    i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,  // Updated format
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 1024,
    .use_apll = false
    };
    i2s_driver_install(I2S_NUM, &i2s_config, 0, NULL);
    i2s_set_pin(I2S_NUM_0, &pin_config);

    // Update OLED
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println(F("Decibel Meter"));
    display.println(F("Waiting for data..."));
    display.display();


    Serial.printf("getID:0x%x\n", PMU.getChipID());

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

    // Disable all interrupts
    PMU.disableIRQ(XPOWERS_AXP2101_ALL_IRQ);
    // Clear all interrupt flags
    PMU.clearIrqStatus();
    // Enable the required interrupt function
    PMU.enableIRQ(
        XPOWERS_AXP2101_BAT_INSERT_IRQ    | XPOWERS_AXP2101_BAT_REMOVE_IRQ      |   // BATTERY
        XPOWERS_AXP2101_VBUS_INSERT_IRQ   | XPOWERS_AXP2101_VBUS_REMOVE_IRQ     |   // VBUS
        XPOWERS_AXP2101_PKEY_SHORT_IRQ    | XPOWERS_AXP2101_PKEY_LONG_IRQ       |   // POWER KEY
        XPOWERS_AXP2101_BAT_CHG_DONE_IRQ  | XPOWERS_AXP2101_BAT_CHG_START_IRQ       // CHARGE
    );

    /*********************************
     * step 4 : Set PMU Charger params
    ***********************************/
    // Set the precharge charging current
    PMU.setPrechargeCurr(XPOWERS_AXP2101_PRECHARGE_50MA);
    // Set constant current charge current limit , 0mA disable charge
    PMU.setChargerConstantCurr(XPOWERS_AXP2101_CHG_CUR_0MA);
    // Set charge cut-off voltage
    PMU.setChargeTargetVoltage(XPOWERS_AXP2101_CHG_VOL_4V2);


    /*********************************
     * step 5 : Disable all power channel , Used to monitor charging current and exclude other load current consumption
    ***********************************/
    PMU.disableDC2();
    PMU.disableDC3();
    PMU.disableDC4();
    PMU.disableDC5();

    PMU.disableALDO1();
    PMU.disableALDO2();
    PMU.disableALDO3();
    PMU.disableALDO4();
    PMU.disableBLDO1();
    PMU.disableBLDO2();

    PMU.disableCPUSLDO();
    PMU.disableDLDO1();
    PMU.disableDLDO2();

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

    /*
    The default setting is CHGLED is automatically controlled by the PMU.
    - XPOWERS_CHG_LED_OFF,
    - XPOWERS_CHG_LED_BLINK_1HZ,
    - XPOWERS_CHG_LED_BLINK_4HZ,
    - XPOWERS_CHG_LED_ON,
    - XPOWERS_CHG_LED_CTRL_CHG,
    * */
    PMU.setChargingLedMode(XPOWERS_CHG_LED_BLINK_4HZ);

    // Print default setting current
    printChargerConstantCurr();

    pinMode(USER_BUTTON_PIN, INPUT_PULLUP);
    pinMode(PIEZO_PIN, OUTPUT);
}

uint8_t curIndex = 4;

void saveCalibration(float ref) {
    calibrationReference = ref;
    isCalibrated = true;
    // Optionally, store in EEPROM for persistence
}

void calibrateMicrophone() {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println(F("Calibrating..."));
    display.display();

    delay(500);

    int refFreq = 1000; // 1 kHz reference tone
    int duration = CALIBRATION_DURATION_MS; // 1 second

    tone(PIEZO_PIN, refFreq);

    unsigned long start = millis();
    double sum = 0;
    size_t totalSamples = 0;

    while (millis() - start < duration) {
        size_t bytesRead = 0;
        i2s_read(I2S_NUM, buffer, BUFFER_SIZE * sizeof(int16_t), &bytesRead, 100);
        int samples = bytesRead / sizeof(int16_t);
        filterAWeighting(buffer, filtered, samples);
        for (int i = 0; i < samples; i++) {
            sum += filtered[i] * filtered[i];
        }
        totalSamples += samples;
    }

    noTone(PIEZO_PIN);

    float rms = (totalSamples > 0) ? sqrt(sum / totalSamples) : 0;
    saveCalibration(rms);

    display.clearDisplay();
    display.setCursor(0, 0);
    display.println(F("Calibration Done"));
    display.printf("Ref RMS: %.4f", rms);
    display.display();
    delay(1500);
}

void loop()
{
    const int MEASURE_WINDOW_MS = 500; // Measurement window in ms
    unsigned long start = millis();
    double sum = 0;
    size_t totalSamples = 0;

    // Accumulate samples for the measurement window
    while (millis() - start < MEASURE_WINDOW_MS) {
        size_t bytesRead = 0;
        i2s_read(I2S_NUM, buffer, BUFFER_SIZE * sizeof(int16_t), &bytesRead, 100);
        int samples = bytesRead / sizeof(int16_t);
        filterAWeighting(buffer, filtered, samples);
        for (int i = 0; i < samples; i++) {
            sum += filtered[i] * filtered[i];
        }
        totalSamples += samples;
    }

    float rms = (totalSamples > 0) ? sqrt(sum / totalSamples) : 0;

    // Calcula dB SPL aproximado
    float db_spl = 0;
    float db = 0;
    if (isCalibrated && calibrationReference > 0.0) {
        db = 20 * log10(rms / calibrationReference);
        db_spl = db + PIEZO_SPL_DB;
    } else {
        db = 20 * log10(rms);
        db_spl = db + PIEZO_SPL_DB;
    }

    // Update OLED
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println(F("Decibel Meter (A)"));
    display.setTextSize(1);
    display.printf("RMS: %.4f\n", rms);
    display.printf("dB: %.2f\n", db);
    display.printf("dB SPL: %.2f\n", db_spl);
    if (!isCalibrated) {
        display.println(F("Not Calibrated!"));
    }
    display.display();

    // Button handling
    static unsigned long buttonPressTime = 0;
    static bool buttonWasPressed = false;
    bool buttonPressed = (digitalRead(USER_BUTTON_PIN) == LOW);

    if (buttonPressed && !buttonWasPressed) {
        buttonPressTime = millis();
    }
    if (!buttonPressed && buttonWasPressed) {
        unsigned long pressDuration = millis() - buttonPressTime;
        if (pressDuration > 1000) { // Long press: calibrate
            calibrateMicrophone();
        } else { // Short press: play melody
            PMU.setChargingLedMode(XPOWERS_CHG_LED_ON);
            for (int i = 0; i < 8; i++) {
                int noteDuration = 1000 / noteDurations[i];
                tone(PIEZO_PIN, melody[i], noteDuration);
                int pauseBetweenNotes = noteDuration * 1.30;
                delay(pauseBetweenNotes);
                noTone(PIEZO_PIN);
            }
            PMU.setChargingLedMode(XPOWERS_CHG_LED_OFF);
        }
    }
    buttonWasPressed = buttonPressed;

    // Small delay to debounce the button
    delay(50);

    // ...existing PMU interrupt code...
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
