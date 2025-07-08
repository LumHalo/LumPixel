#include "LumaPixel.h"

LumaPixel::LumaPixel(RGB* pixelBuffer, int numLeds, int gpioPin, rmt_channel_t channel)
  : buffer(pixelBuffer), ledCount(numLeds), gpio(gpioPin), rmtChannel(channel)
{
  rmtItems = new rmt_item32_t[ledCount * bitsPerLed];
  initGammaTable();
  setupRMT();
}

void LumaPixel::setupRMT() {
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

RGBW LumaPixel::RGBtoRGBW(const RGB& rgb) {
  // Correction couleur
  float r_corr = rgb.r * CORRECTION_R;
  float g_corr = rgb.g * CORRECTION_G;
  float b_corr = rgb.b * CORRECTION_B;

  // Clamp
  r_corr = min(255.0f, r_corr);
  g_corr = min(255.0f, g_corr);
  b_corr = min(255.0f, b_corr);

  // Correction gamma (avec table)
  uint8_t r_gamma = gammaTable[(int)r_corr];
  uint8_t g_gamma = gammaTable[(int)g_corr];
  uint8_t b_gamma = gammaTable[(int)b_corr];

  // Extraction du blanc commun
  uint8_t w = min(r_gamma, min(g_gamma, b_gamma));

  return {
    (uint8_t)(r_gamma - w),
    (uint8_t)(g_gamma - w),
    (uint8_t)(b_gamma - w),
    w
  };
}


void LumaPixel::show() {
  int itemIndex = 0;

  for (int i = 0; i < ledCount; i++) {
    RGBW c = RGBtoRGBW(buffer[i]);
    uint8_t bytes[4] = { c.g, c.r, c.b, c.w };

    for (int j = 0; j < 4; j++) {
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

void LumaPixel::initGammaTable() {
  for (int i = 0; i < 256; i++) {
    gammaTable[i] = pow(i / 255.0, GAMMA) * 255 + 0.5;
  }
}
