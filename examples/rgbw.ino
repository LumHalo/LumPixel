#include <LumPixel.h>

#define LED_PIN 4
#define NUM_LEDS 20

LumPixel strip(NUM_LEDS, LED_PIN, MODE_RGB, RMT_CHANNEL_0);

void setup() {}

void loop() {
  // Rouge
  for (int i = 0; i < NUM_LEDS; i++) strip.setLed(i, 255, 0, 0);
  strip.show();
  delay(1000);

  // Vert
  strip.fill(0, 255, 0);
  strip.show();
  delay(1000);

  // Bleu
  strip.fill(0, 0, 255);
  strip.show();
  delay(1000);

  // Blanc
  strip.fill(255, 255, 255);
  strip.show();
  delay(1000);
}
