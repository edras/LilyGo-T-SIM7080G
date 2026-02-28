#pragma once
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

extern QueueHandle_t i2sEventQueue;

void initAudio();
// Blocking: reads exactly `count` samples from I2S DMA into `buf`.
// Returns actual samples read (may be <= count).
size_t readAudioSamples(int16_t *buf, size_t count, TickType_t waitTicks);
