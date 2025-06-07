#include <Arduino.h>
#include "audio_processing.h"
#include "display_utils.h"
#include "power_utils.h"
#include "melody_utils.h"

void setup() {
    
    Serial.begin(115200);
    while (!Serial);
    delay(3000);

    initPMU();
    initDisplay();
    initAudio();
    initMelody();

    displaySplash();
}

void loop() {
    static bool buttonWasPressed = false;
    static unsigned long buttonPressTime = 0;

    // --- Audio Measurement ---
    float rms = measureRMS(MEASURE_WINDOW_MS);
    float db = calculateDB(rms);
    float db_spl = calculateDBSPL(rms);

    // --- Display ---
    displayMeasurement(rms, db, db_spl, isCalibrated);

    // --- Button Handling ---
    bool buttonPressed = (digitalRead(USER_BUTTON_PIN) == LOW);
    if (buttonPressed && !buttonWasPressed) {
        buttonPressTime = millis();
    }
    if (!buttonPressed && buttonWasPressed) {
        unsigned long pressDuration = millis() - buttonPressTime;
        if (pressDuration > 1000) {
            calibrateMicrophone();
        } else {
            playMelody();
        }
    }
    buttonWasPressed = buttonPressed;

    // --- PMU Interrupts ---
    handlePMUInterrupt();

    delay(50);
}