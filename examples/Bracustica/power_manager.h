#pragma once
#include <Arduino.h>

void initPMU();
void enableLightSleep();
void enableModemPower();
void disableModemPower();
void enableGPSPower();
void disableGPSPower();
void enableSDPower();
void disableSDPower();
void modemSleep();
void modemWake();
void handlePMUInterrupt();
float getBatteryVoltage();
