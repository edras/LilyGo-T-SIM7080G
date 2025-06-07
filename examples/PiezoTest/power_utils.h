#pragma once
#include <Arduino.h>

void initPMU();
void handlePMUInterrupt();
void setChargingLedOn();
void setChargingLedOff();
void setChargingLed4Hz();
void setChargingLed1Hz();