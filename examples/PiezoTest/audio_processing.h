#pragma once
#include <Arduino.h>

#define SAMPLE_RATE 8000
#define BUFFER_SIZE 256 // or 512
#define CALIBRATION_DURATION_MS 1000
#define MEASURE_WINDOW_MS 500
#define PIEZO_SPL_DB 80.0

extern float calibrationReference;
extern bool isCalibrated;

void initAudio();
float measureRMS(int window_ms);
float calculateDB(float rms);
float calculateDBSPL(float rms);
void calibrateMicrophone();
void filterAWeighting(int16_t *in, float *out, int length);