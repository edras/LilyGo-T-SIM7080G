#include "audio_processing.h"
#include <driver/i2s.h>
#include "display_utils.h"
#include "melody_utils.h"

float calibrationReference = 1.0;
bool isCalibrated = false;

int16_t buffer[BUFFER_SIZE];
float filtered[BUFFER_SIZE];

struct Biquad {
    float a0, a1, a2, b1, b2;
    float z1, z2;
};

Biquad aweight_biquad = {
    1.0f, -2.0f, 1.0f,
    -1.99004745483398f, 0.99007225036621f,
    0, 0
};

void filterAWeighting(int16_t *in, float *out, int length) {
    aweight_biquad.z1 = aweight_biquad.z2 = 0;
    for (int i = 0; i < length; i++) {
        float sample = in[i] / 32768.0f;
        float result = aweight_biquad.a0 * sample + aweight_biquad.z1;
        aweight_biquad.z1 = aweight_biquad.a1 * sample - aweight_biquad.b1 * result + aweight_biquad.z2;
        aweight_biquad.z2 = aweight_biquad.a2 * sample - aweight_biquad.b2 * result;
        out[i] = result;
    }
}

float measureRMS(int window_ms) {
    unsigned long start = millis();
    double sum = 0;
    size_t totalSamples = 0;
    while (millis() - start < window_ms) {
        size_t bytesRead = 0;
        i2s_read(I2S_NUM_0, buffer, BUFFER_SIZE * sizeof(int16_t), &bytesRead, 100);
        int samples = bytesRead / sizeof(int16_t);
        filterAWeighting(buffer, filtered, samples);
        for (int i = 0; i < samples; i++) {
            sum += filtered[i] * filtered[i];
        }
        totalSamples += samples;
    }
    return (totalSamples > 0) ? sqrt(sum / totalSamples) : 0;
}

float calculateDB(float rms) {
    if (isCalibrated && calibrationReference > 0.0) {
        return 20 * log10(rms / calibrationReference);
    } else {
        return 20 * log10(rms);
    }
}

float calculateDBSPL(float rms) {
    return calculateDB(rms) + PIEZO_SPL_DB;
}

void calibrateMicrophone() {
    displayCalibrating();
    delay(500);

    int refFreq = 1000;
    int duration = CALIBRATION_DURATION_MS;
    tone(PIEZO_PIN, refFreq);

    unsigned long start = millis();
    double sum = 0;
    size_t totalSamples = 0;
    while (millis() - start < duration) {
        size_t bytesRead = 0;
        i2s_read(I2S_NUM_0, buffer, BUFFER_SIZE * sizeof(int16_t), &bytesRead, 100);
        int samples = bytesRead / sizeof(int16_t);
        filterAWeighting(buffer, filtered, samples);
        for (int i = 0; i < samples; i++) {
            sum += filtered[i] * filtered[i];
        }
        totalSamples += samples;
    }
    noTone(PIEZO_PIN);

    float rms = (totalSamples > 0) ? sqrt(sum / totalSamples) : 0;
    calibrationReference = rms;
    isCalibrated = true;
    displayCalibrationDone(rms);
}
void initAudio() {
    i2s_pin_config_t pin_config = {
        .bck_io_num = 14,
        .ws_io_num = 36,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = 11,
    };
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = 1024,
        .use_apll = false
    };
    i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
    i2s_set_pin(I2S_NUM_0, &pin_config);
}