#ifndef LUMAPIXEL_H
#define LUMAPIXEL_H

#pragma once
#include "driver/rmt.h"
#include "esp_log.h"
#include <Arduino.h>

#define RMT_CLK_DIV 2        // 80MHz / 2 = 40MHz (25ns par tick)
#define T0H  12              // 0.3us / 25ns
#define T0L  34              // 0.85us / 25ns
#define T1H  28              // 0.7us / 25ns
#define T1L  18              // 0.6us / 25ns
#define RESET_US 80          // 80µs de reset

#define CORRECTION_R 1.0f
#define CORRECTION_G 1.0f
#define CORRECTION_B 1.0f

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
    LumPixel(RGB* pixelBuffer, int numLeds, int gpioPin, LedMode mode = MODE_RGB, rmt_channel_t channel = RMT_CHANNEL_0);
    void show();
    void setLed(int led, uint8_t r, uint8_t g, uint8_t b);
    void fill(uint8_t r, uint8_t g, uint8_t b);

private:
    void initGammaTable();
    void setupRMT();

    const float GAMMA = 2.2f;

    RGB* buffer;
    int ledCount;
    int gpio;
    rmt_channel_t rmtChannel;

    uint8_t gammaTableR[256];
    uint8_t gammaTableG[256];
    uint8_t gammaTableB[256];
    
    LedMode ledMode;
    int bitsPerLed;
    int bytesPerLed;

    rmt_item32_t* rmtItems;
};

#endif