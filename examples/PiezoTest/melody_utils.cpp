#include "melody_utils.h"
#include "power_utils.h"

int melody[] = {262, 294, 330, 349, 392, 440, 494, 523};
int noteDurations[] = {4, 4, 4, 4, 4, 4, 4, 4};

void initMelody() {
    pinMode(USER_BUTTON_PIN, INPUT_PULLUP);
    pinMode(PIEZO_PIN, OUTPUT);
}

void playMelody() {
    setChargingLedOn();
    // Play a simple melody
    // C D E F G A B C
    for (int i = 0; i < 8; i++) {
        int noteDuration = 1000 / noteDurations[i];
        tone(PIEZO_PIN, melody[i], noteDuration);
        int pauseBetweenNotes = noteDuration * 1.30;
        delay(pauseBetweenNotes);
        noTone(PIEZO_PIN);
    }
    setChargingLedOff();
}