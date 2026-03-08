#include "LumPixel.h"
#include "driver/rmt.h"
#include "esp_log.h"
#include <Arduino.h>

LumPixel::LumPixel(int numLeds, int gpioPin, LedMode mode, rmt_channel_t channel)
  : numLeds(numLeds), gpio(gpioPin), ledMode(mode), rmtChannel(channel)
{
  bufferMain = new RGB[numLeds];
  bufferActive = new RGB[numLeds];
  memset(bufferMain, 0, numLeds * sizeof(RGB));
  memset(bufferActive, 0, numLeds * sizeof(RGB));

  bytesPerLed = (ledMode == MODE_RGBW) ? 4 : 3;
  bitsPerLed = bytesPerLed * 8;

  rmtItems = new rmt_item32_t[numLeds * bitsPerLed];
  _mutex = xSemaphoreCreateMutex();
  initGammaTable();
  setupRMT();

  xTaskCreatePinnedToCore(
    this->renderTask,   // Fonction de la tâche
    "LED_Dither",       // Nom
    4096,               // Stack size
    this,               // Passer l'instance actuelle
    3,                  // Priorité (assez haute)
    &_taskHandle,       // Handle
    1                   // Core 1
  );
}

LumPixel::~LumPixel() {
    if (_taskHandle != NULL) {
      vTaskDelete(_taskHandle);
    }
    rmt_driver_uninstall(rmtChannel);

    if (_mutex != NULL) {
      vSemaphoreDelete(_mutex);
    }
    delete[] bufferMain;
    delete[] bufferActive;
    delete[] rmtItems;
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
  std::fill(bufferMain, bufferMain + numLeds, RGB{ r, g, b });
}

void LumPixel::setLed(int led, uint8_t r, uint8_t g, uint8_t b) {
  if (led < numLeds) {
    bufferMain[led] = RGB{ r, g, b };
  }
}

void LumPixel::show() {
  if (xSemaphoreTake(_mutex, portMAX_DELAY)) {
    memcpy(bufferActive, bufferMain, numLeds * sizeof(RGB));
    xSemaphoreGive(_mutex);
  }
}

void LumPixel::initGammaTable() {
  for (int i = 0; i < 256; i++) {
    float input = (float)i / 255.0f;
    float brightness = powf(input, GAMMA);

    float r = brightness * CORRECTION_R * 65535.0f;
    float g = brightness * CORRECTION_G * 65535.0f;
    float b = brightness * CORRECTION_B * 65535.0f;

    gammaTableR[i] = (uint16_t)std::min(65535.0f, r + 0.5f);
    gammaTableG[i] = (uint16_t)std::min(65535.0f, g + 0.5f);
    gammaTableB[i] = (uint16_t)std::min(65535.0f, b + 0.5f);
  }
}

void LumPixel::renderTask(void* pvParameters) {
  LumPixel* instance = (LumPixel*)pvParameters;
  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xFrequence = pdMS_TO_TICKS(10);

  for (;;) {
    instance->internalShow();
    vTaskDelayUntil(&xLastWakeTime, xFrequence);
  }
}

void LumPixel::internalShow() {
  int itemIndex = 0;
  ditherFrameCounter++;

  if (xSemaphoreTake(_mutex, portMAX_DELAY)) {
    for (int i = 0; i < numLeds; i++) {
      uint16_t valR = gammaTableR[bufferActive[i].r];
      uint16_t valG = gammaTableG[bufferActive[i].g];
      uint16_t valB = gammaTableB[bufferActive[i].b];

      /* uint8_t r = (valR >> 8) + ((uint8_t)(valR & 0xFF) > ditherFrameCounter ? 1 : 0);
      uint8_t g = (valG >> 8) + ((uint8_t)(valG & 0xFF) > ditherFrameCounter ? 1 : 0);
      uint8_t b = (valB >> 8) + ((uint8_t)(valB & 0xFF) > ditherFrameCounter ? 1 : 0); */
      uint8_t rBase = valR >> 8;
      uint8_t gBase = valG >> 8;
      uint8_t bBase = valB >> 8;

      uint8_t r = (rBase == 255) ? 255 : rBase + ((valR & 0xFF) > ditherFrameCounter ? 1 : 0);
      uint8_t g = (gBase == 255) ? 255 : gBase + ((valG & 0xFF) > ditherFrameCounter ? 1 : 0);
      uint8_t b = (bBase == 255) ? 255 : bBase + ((valB & 0xFF) > ditherFrameCounter ? 1 : 0);

      uint8_t w = 0;
      if (ledMode == MODE_RGBW) {
          w = std::min(r, std::min(g, b));
          r -= w; g -= w; b -= w;
      }

      uint8_t bytes[4] = { g, r, b, w };

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
    xSemaphoreGive(_mutex);
  }
  rmt_write_items(rmtChannel, rmtItems, itemIndex, true);
}