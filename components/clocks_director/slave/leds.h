#pragma once

#include <APA102.h>

#include "../clocks_shared/stub.h"

#define LEDS_ERROR_MISSING_APA102_USE_FAST_GPIO 1
#define LEDS_ERROR_NO_INTERRUPT 2
#define LEDS_ERROR_TIMER2_FAILED 3

typedef rgb_color RgbLeds[LED_COUNT];
typedef uint16_t BrightnessLeds[LED_COUNT];

class LedColors {
 public:
  static rgb_color purple;
  static rgb_color orange;
  static rgb_color blue;
  static rgb_color red;
  static rgb_color green;
  static rgb_color black;
};

namespace Leds {
void setup();

void blink(const rgb_color &color, int leds = LED_COUNT);

void error(int leds);

void set_ex(uint8_t led, const rgb_color &color);
void set_all_ex(const rgb_color &color);

void publish();
};  // namespace Leds

#define LED_TIMER 1
#define LED_SYNC_IN 2
#define LED_ONEWIRE 3
#define LED_SYNC_OUT 4
#define LED_MODE 7

#define LED_CHANNEL_STATE 11
#define LED_CHANNEL_DATA 10

#define LED_EXECUTOR_STATE 9
