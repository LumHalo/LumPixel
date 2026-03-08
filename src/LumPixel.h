#ifndef LUMAPIXEL_H
#define LUMAPIXEL_H

#pragma once
#include "driver/rmt.h"
#include "esp_log.h"
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

#define RMT_CLK_DIV 2        // 80MHz / 2 = 40MHz (25ns par tick)
#define T0H  12              // 0.3us / 25ns
#define T0L  34              // 0.85us / 25ns
#define T1H  28              // 0.7us / 25ns
#define T1L  18              // 0.6us / 25ns
#define RESET_US 80          // 80µs de reset

static uint8_t gammaTable[256];

enum LedMode {
    MODE_RGB, // 24 bits
    MODE_RGBW // 32 bits
};

struct RGB {
    uint8_t r, g, b;
};

struct RGBW {
    uint8_t r, g, b, w;
};

class LumPixel {
public:
    LumPixel(int numLeds, int gpioPin, LedMode mode = MODE_RGB, rmt_channel_t channel = RMT_CHANNEL_0);
    ~LumPixel();

    void setDithering(bool active) { _ditheringEnabled = active; }

    void show();
    void setLed(int led, uint8_t r, uint8_t g, uint8_t b);
    void fill(uint8_t r, uint8_t g, uint8_t b);

private:
    TaskHandle_t _taskHandle = NULL;
    SemaphoreHandle_t _mutex;

    bool _ditheringEnabled = true;
    uint32_t *accR, *accG, *accB, *accW;

    static void renderTask(void* pvParameters);
    void internalShow();
    void initGammaTable();
    void setupRMT();

    float MIN_VISIBLE = 0.0f;
    float GAMMA = 2.2f;
    float CORRECTION_R = 1.0f;
    float CORRECTION_G = 1.0f; //0.65
    float CORRECTION_B = 1.0f; //0.75

    RGB* bufferMain;
    RGB* bufferActive;
    int numLeds;
    int gpio;
    rmt_channel_t rmtChannel;

    uint16_t gammaTableR[256];
    uint16_t gammaTableG[256];
    uint16_t gammaTableB[256];
    
    LedMode ledMode;
    int bitsPerLed;
    int bytesPerLed;

    rmt_item32_t* rmtItems;
};

#endif