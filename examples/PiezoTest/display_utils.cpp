#include "display_utils.h"

Adafruit_SSD1306 display(128, 64, &Wire, -1);

void initDisplay() {
    Wire.begin(17, 16);
    display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println(F("Initializing..."));
    display.display();
}

void displaySplash() {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println(F("Decibel Meter"));
    display.println(F("Waiting for data..."));
    display.display();
}

void displayMeasurement(float rms, float db, float db_spl, bool calibrated) {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println(F("Decibel Meter (A)"));
    display.setTextSize(1);
    display.printf("RMS: %.4f\n", rms);
    display.printf("dB: %.2f\n", db);
    display.printf("dB SPL: %.2f\n", db_spl);
    if (!calibrated) {
        display.println(F("Not Calibrated!"));
    }
    display.display();
}

void displayCalibrating() {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println(F("Calibrating..."));
    display.display();
}

void displayCalibrationDone(float rms) {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println(F("Calibration Done"));
    display.printf("Ref RMS: %.4f", rms);
    display.display();
    delay(1500);
}