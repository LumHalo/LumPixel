#include "LumPixel.h"
#include "driver/rmt.h"
#include "esp_log.h"
#include <Arduino.h>

LumPixel::LumPixel(RGB* pixelBuffer, int numLeds, int gpioPin, LedMode mode, rmt_channel_t channel)
  : buffer(pixelBuffer), ledCount(numLeds), gpio(gpioPin), ledMode(mode), rmtChannel(channel)
{
  bytesPerLed = (ledMode == MODE_RGBW) ? 4 : 3;
  bitsPerLed = bytesPerLed * 8;

  rmtItems = new rmt_item32_t[ledCount * bitsPerLed];
  initGammaTable();
  setupRMT();
}

void LumPixel::setupRMT() {
  rmt_config_t config;
  config.rmt_mode = RMT_MODE_TX;
  config.channel = rmtChannel;
  config.gpio_num = (gpio_num_t)gpio;
  config.mem_block_num = 1;
  config.clk_div = RMT_CLK_DIV;
  config.tx_config.loop_en = false;
  config.tx_config.carrier_en = false;
  config.tx_config.idle_output_en = true;
  config.tx_config.idle_level = RMT_IDLE_LEVEL_LOW;

  rmt_config(&config);
  rmt_driver_install(rmtChannel, 0, 0);
}

void LumPixel::fill(uint8_t r, uint8_t g, uint8_t b) {
  std::fill(buffer, buffer + ledCount, RGB{ r, g, b });
}

void LumPixel::setLed(int led, uint8_t r, uint8_t g, uint8_t b) {
  buffer[led] = RGB{ r, g, b };
}

void LumPixel::show() {
  int itemIndex = 0;

  for (int i = 0; i < ledCount; i++) {
    uint8_t r = gammaTableR[buffer[i].r];
    uint8_t g = gammaTableG[buffer[i].g];
    uint8_t b = gammaTableB[buffer[i].b];
    uint8_t w = 0;

    if (ledMode == MODE_RGBW) {
      w = std::min(r, std::min(g, b));
      r -= w;
      g -= w;
      b -= w;
    }
    uint8_t bytes[4] = { b, r, g, w };

    for (int j = 0; j < bytesPerLed; j++) {      
      for (int bit = 7; bit >= 0; bit--) {
        bool isOne = bytes[j] & (1 << bit);
        rmtItems[itemIndex].duration0 = isOne ? T1H : T0H;
        rmtItems[itemIndex].level0 = 1;
        rmtItems[itemIndex].duration1 = isOne ? T1L : T0L;
        rmtItems[itemIndex].level1 = 0;
        itemIndex++;
      }
    }
  }

  rmt_write_items(rmtChannel, rmtItems, itemIndex, true);
  rmt_wait_tx_done(rmtChannel, portMAX_DELAY);
  delayMicroseconds(RESET_US);
}

void LumPixel::initGammaTable() {
  for (int i = 0; i < 256; i++) {
    float r_corr = std::min(255.0f, i * CORRECTION_R);
    float g_corr = std::min(255.0f, i * CORRECTION_G);
    float b_corr = std::min(255.0f, i * CORRECTION_B);

    gammaTableR[i] = (uint8_t)(pow(r_corr / 255.0f, GAMMA) * 255.0f + 0.5f);
    gammaTableG[i] = (uint8_t)(pow(g_corr / 255.0f, GAMMA) * 255.0f + 0.5f);
    gammaTableB[i] = (uint8_t)(pow(b_corr / 255.0f, GAMMA) * 255.0f + 0.5f);
  }
}
