#pragma once
#include <Adafruit_SSD1306.h>

extern Adafruit_SSD1306 display;

void initDisplay();
void displaySplash();
void displayMeasurement(float rms, float db, float db_spl, bool calibrated);
void displayCalibrating();
void displayCalibrationDone(float rms);