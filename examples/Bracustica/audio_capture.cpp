#include "audio_capture.h"
#include "config.h"
#include <driver/i2s.h>

QueueHandle_t i2sEventQueue = nullptr;

void initAudio() {
    i2s_config_t cfg = {
        .mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate          = SAMPLE_RATE,
        .bits_per_sample      = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format       = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags     = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count        = I2S_DMA_BUF_COUNT,
        .dma_buf_len          = I2S_DMA_BUF_LEN,
        .use_apll             = false,
    };
    i2s_driver_install(I2S_NUM_0, &cfg, 4, &i2sEventQueue);

    i2s_pin_config_t pins = {
        .bck_io_num   = I2S_BCK_PIN,
        .ws_io_num    = I2S_WS_PIN,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num  = I2S_DATA_IN_PIN,
    };
    i2s_set_pin(I2S_NUM_0, &pins);
}

size_t readAudioSamples(int16_t *buf, size_t count, TickType_t waitTicks) {
    size_t bytesRead = 0;
    i2s_read(I2S_NUM_0, buf, count * sizeof(int16_t), &bytesRead, waitTicks);
    return bytesRead / sizeof(int16_t);
}
