#include "LumPixel.h"

LumPixel::LumPixel(int numLeds, int gpioPin, LedMode mode, rmt_channel_t channel)
  : numLeds(numLeds), gpio(gpioPin), ledMode(mode), rmtChannel(channel)
{
  bufferMain = new RGB[numLeds];
  bufferActive = new RGB[numLeds];
  bufferRender = new RGB[numLeds];
  memset(bufferMain, 0, numLeds * sizeof(RGB));
  memset(bufferActive, 0, numLeds * sizeof(RGB));
  memset(bufferRender, 0, numLeds * sizeof(RGB));

  for(int v=0; v<256; v++) {
      for(int bit=0; bit<8; bit++) {
          bool one = v & (1<<(7-bit));

          rmtBitTable[v][bit].level0 = 1;
          rmtBitTable[v][bit].duration0 = one ? T1H : T0H;
          rmtBitTable[v][bit].level1 = 0;
          rmtBitTable[v][bit].duration1 = one ? T1L : T0L;
      }
  }

  accR = new int32_t[numLeds]();
  accG = new int32_t[numLeds]();
  accB = new int32_t[numLeds]();
  if (ledMode == MODE_RGBW) accW = new int32_t[numLeds]();
  else accW = nullptr;

  bytesPerLed = (ledMode == MODE_RGBW) ? 4 : 3;
  bitsPerLed = bytesPerLed * 8;

  rmtItems = new rmt_item32_t[numLeds * bitsPerLed + 1];
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
  delete[] bufferRender;
  delete[] rmtItems;

  delete[] accR;
  delete[] accG;
  delete[] accB;
  if (accW) delete[] accW;
}

void LumPixel::setupRMT() {
  rmt_config_t config;
  config.rmt_mode = RMT_MODE_TX;
  config.channel = rmtChannel;
  config.gpio_num = (gpio_num_t)gpio;
  config.mem_block_num = 2;
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
    RGB* tmp = bufferActive;
    bufferActive = bufferMain;
    bufferMain = tmp;
    frameDirty = true;
    xSemaphoreGive(_mutex);
  }
}

void LumPixel::initGammaTable() {
  for (int i = 0; i < 256; i++) {
    if (i == 0) {
      gammaTableR[0] = gammaTableG[0] = gammaTableB[0] = 0;
      continue;
    }

    float input = (float)i / 255.0f;
    float brightness = powf(input, GAMMA);

    float r = brightness * CORRECTION_R * 65535.0f;
    float g = brightness * CORRECTION_G * 65535.0f;
    float b = brightness * CORRECTION_B * 65535.0f;

    gammaTableR[i] = (uint16_t)std::max(MIN_VISIBLE, std::min(65535.0f, r + 0.5f));
    gammaTableG[i] = (uint16_t)std::max(MIN_VISIBLE, std::min(65535.0f, g + 0.5f));
    gammaTableB[i] = (uint16_t)std::max(MIN_VISIBLE, std::min(65535.0f, b + 0.5f));
  }

  for (int i = 0; i < 50; ++i) {
    Serial.printf("i=%d -> R16=%u R8=%u\n", i, gammaTableR[i], gammaTableR[i] >> 8);
  }
}

void LumPixel::setGamma(float gamma, float cr = 1.0f, float cg = 1.0f, float cb = 1.0f) {
    GAMMA = gamma; CORRECTION_R = cr; CORRECTION_G = cg; CORRECTION_B = cb;
    initGammaTable();
    memset(accR, 0, numLeds * sizeof(int32_t));
    memset(accG, 0, numLeds * sizeof(int32_t));
    memset(accB, 0, numLeds * sizeof(int32_t));
}

void LumPixel::renderTask(void* pvParameters) {
  LumPixel* instance = (LumPixel*)pvParameters;
  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xFrequence = pdMS_TO_TICKS(16);

  for (;;) {
    /* if (instance->frameDirty) {
      instance->internalShow();
      instance->frameDirty = false;
    } */
    instance->internalShow();
    vTaskDelayUntil(&xLastWakeTime, xFrequence);
  }
}

uint32_t LumPixel::fastRand() {
    _rngState ^= _rngState << 13;
    _rngState ^= _rngState >> 17;
    _rngState ^= _rngState << 5;
    return _rngState;
}

inline uint8_t LumPixel::ditherChannel(int32_t &acc, uint32_t val16) {
    if (val16 > 256 && val16 < 65279) {
        int32_t noise = ((int32_t)(fastRand() & 0x1FF)) - 256;
        val16 += noise >> 2;
    }

    acc += val16;
    int32_t out = acc >> 8;

    if (out <= 0) { acc -= 0; return 0; }
    if (out >= 255) { acc -= (255<<8); return 255; }

    acc -= (out << 8);
    return (uint8_t)out;
}

void LumPixel::internalShow() {
  const int maxItems = numLeds * bitsPerLed + 1;
  int itemIndex = 0;

  if (xSemaphoreTake(_mutex, portMAX_DELAY)) {
      memcpy(bufferRender, bufferActive, numLeds * sizeof(RGB));
      xSemaphoreGive(_mutex);
  }
  RGB* render = bufferRender;

  for (int i = 0; i < numLeds; i++) {
    uint32_t valR = gammaTableR[render[i].r];
    uint32_t valG = gammaTableG[render[i].g];
    uint32_t valB = gammaTableB[render[i].b];

    uint32_t valW = 0;
    if (ledMode == MODE_RGBW) {
      uint32_t minRGB = std::min(valR, std::min(valG, valB));
      valW = minRGB;
      valR -= valW; valG -= valW; valB -= valW;
    }

    uint8_t rOut, gOut, bOut, wOut = 0;

    if (_ditheringEnabled) {
      rOut = ditherChannel(accR[i], valR);
      gOut = ditherChannel(accG[i], valG);
      bOut = ditherChannel(accB[i], valB);
      if (ledMode == MODE_RGBW) wOut = ditherChannel(accW[i], valW);
    } else {
      rOut = (uint8_t)(valR >> 8);
      gOut = (uint8_t)(valG >> 8);
      bOut = (uint8_t)(valB >> 8);
      if (ledMode == MODE_RGBW) wOut = (uint8_t)(valW >> 8);
    }

    uint8_t bytes[4] = { bOut, rOut, gOut, wOut };

    for (int j = 0; j < bytesPerLed; j++) {
      if (itemIndex + 8 > maxItems) {
        itemIndex = maxItems - 1;
        break;
      }
      memcpy(&rmtItems[itemIndex], rmtBitTable[bytes[j]], sizeof(rmt_item32_t) * 8);
      itemIndex += 8;
    }
  }

  if (itemIndex < maxItems) {
    rmt_item32_t resetItem;
    resetItem.level0 = 0;
    resetItem.duration0 = RESET_US * 40;
    resetItem.level1 = 0;
    resetItem.duration1 = 0;
    rmtItems[itemIndex++] = resetItem;
  }

  rmt_write_items(rmtChannel, rmtItems, itemIndex, true);
  rmt_wait_tx_done(rmtChannel, portMAX_DELAY);
}