#pragma once
#include <Arduino.h>
#include "config.h"

#ifdef ENABLE_DISPLAY
void initDisplay();
void displayStatus(float spl, float battV, bool modemOk, bool sdOk);
void displayAlert(float peakSPL);
#else
inline void initDisplay() {}
inline void displayStatus(float, float, bool, bool) {}
inline void displayAlert(float) {}
#endif
