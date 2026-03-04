#include <LumPixel.h>

#define LED_PIN 4
#define NUM_LEDS 20

RGB leds[NUM_LEDS];
LumPixel strip(leds, NUM_LEDS, LED_PIN, MODE_RGB, RMT_CHANNEL_0);

void setup() {}

void loop() {
  // Rouge
  for (int i = 0; i < NUM_LEDS; i++) strip.setLed(i, 255, 0, 0);
  strip.show();
  delay(1000);

  // Vert
  for (int i = 0; i < NUM_LEDS; i++) strip.setLed(i, 0, 255, 0);
  strip.show();
  delay(1000);

  // Bleu
  for (int i = 0; i < NUM_LEDS; i++) strip.setLed(i, 0, 0, 255);
  strip.show();
  delay(1000);

  // Blanc
  for (int i = 0; i < NUM_LEDS; i++) strip.setLed(i, 255, 255, 255);
  strip.show();
  delay(1000);
}
