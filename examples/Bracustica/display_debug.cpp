#include "display_debug.h"
#include "config.h"

#ifdef ENABLE_DISPLAY
#include <Adafruit_SSD1306.h>

static Adafruit_SSD1306 oled(128, 64, &Wire, -1);

void initDisplay() {
    Wire.begin(DISPLAY_SDA, DISPLAY_SCL);
    oled.begin(SSD1306_SWITCHCAPVCC, DISPLAY_ADDR);
    oled.clearDisplay();
    oled.setTextSize(1);
    oled.setTextColor(SSD1306_WHITE);
    oled.setCursor(0, 0);
    oled.println("Bracustica");
    oled.println("Starting...");
    oled.display();
}

void displayStatus(float spl, float battV, bool modemOk, bool sdOk) {
    oled.clearDisplay();
    oled.setCursor(0, 0);
    oled.printf("SPL: %.1f dB\n", spl);
    oled.printf("Batt: %.2fV\n", battV);
    oled.printf("Modem:%s SD:%s\n", modemOk ? "OK" : "--", sdOk ? "OK" : "--");
    oled.display();
}

void displayAlert(float peakSPL) {
    oled.clearDisplay();
    oled.setCursor(0, 0);
    oled.println("!! ALERT !!");
    oled.printf("Peak: %.1f dB\n", peakSPL);
    oled.display();
}
#endif
