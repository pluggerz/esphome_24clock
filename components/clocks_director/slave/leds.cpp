#include "leds.h"

#include "../clocks_shared/pins.h"
#include "leds.background.h"
#include "leds.foreground.h"

uint8_t default_brightness = 31;

using BackgroundLedAnimations::Debug;

BackgroundLedAnimations::Xmas lighting::xmas;
BackgroundLedAnimations::Rainbow lighting::rainbow;
BackgroundLedAnimations::Solid lighting::solid;
BackgroundLedAnimations::Debug lighting::debug;
BackgroundLedAnimations::Individual lighting::individual;
BackgroundLayer *lighting::current = &debug;

EmptyForegroundLayer empty_foreground_led_layer;
FollowHandlesLedLayer follow_led_layer;
ForegroundLayer *current_foreground_layer = &empty_foreground_led_layer;
RgbLeds ForegroundLayer::colors;
BrightnessLeds ForegroundLayer::brightness;
uint8_t FollowHandlesLedLayer::ledAlphas[LED_COUNT];

rgb_color debug_leds[LED_COUNT];
bool dirty = true;

rgb_color LedColors::purple = rgb_color(0xFF, 0x00, 0xFF);
rgb_color LedColors::orange = rgb_color(0xcc, 0x84, 0x00);
rgb_color LedColors::blue = rgb_color(0x00, 0x00, 0xFF);
rgb_color LedColors::red = rgb_color(0xFF, 0x00, 0x00);
rgb_color LedColors::green = rgb_color(0x00, 0xFF, 0x00);
rgb_color LedColors::black = rgb_color(0x00, 0x00, 0x00);

// Create an object for writing to the LED strip.
extern APA102<LED_DATA_PIN, LED_CLOCK_PIN> ledStrip;

bool Debug::update(Millis now) { return dirty; }

void Debug::combine(RgbLeds &result) const {
  for (int idx = 0; idx < LED_COUNT; ++idx) {
    result[idx] = debug_leds[idx];
  }
  dirty = false;
}

void Leds::set_brightness(uint8_t brightness) {
  default_brightness = brightness;
}

void BackgroundLayer::publish() {
  combine(colors);

  // TODO: move to better place
  current_foreground_layer->update(millis());
  BrightnessLeds brightness;
  for (uint8_t idx = 0; idx < LED_COUNT; idx++) {
    brightness[idx] = default_brightness;
    ForegroundLayer::colors[idx] = BackgroundLayer::colors[idx];
  }
  current_foreground_layer->combine(ForegroundLayer::colors, brightness);

  ledStrip.startFrame();
  for (uint8_t idx = 1; idx < LED_COUNT; idx++) {
    ledStrip.sendColor(ForegroundLayer::colors[idx], brightness[idx]);
  }
  ledStrip.sendColor(ForegroundLayer::colors[0], brightness[0]);
  ledStrip.endFrame(LED_COUNT);
}

void Leds::setup() {
  blink(rgb_color(0xFF, 0x00, 0x00));
  blink(rgb_color(0x00, 0xFF, 0x00));
  blink(rgb_color(0x00, 0x00, 0xFF));

  Leds::set_all_ex(rgb_color(0x00, 0x00, 0x00));
  Leds::publish();
}

void Leds::error(int leds) {
  while (true) blink(LedColors::red, leds);
}

void Leds::set_all_ex(const rgb_color &color) {
  dirty = true;
  for (auto idx = 0; idx < LED_COUNT; ++idx) {
    debug_leds[idx] = color;
  }
}

void Leds::set_ex(uint8_t led, const rgb_color &color) {
  led = (led - 1) % LED_COUNT;
  dirty = true;
  debug_leds[led] = color;
}

void Leds::publish() {
  dirty = true;
  lighting::current->publish();
}

void set_all_raw(const rgb_color &color, int leds) {
  ledStrip.startFrame();
  const auto brightness = 31;
  for (uint8_t idx = 1; idx < LED_COUNT; idx++) {
    ledStrip.sendColor(idx < leds ? color : LedColors::black, brightness);
  }
  ledStrip.sendColor(1 <= leds ? color : LedColors::black, brightness);
  ledStrip.endFrame(LED_COUNT);
}

int blink_delay = 200;
void Leds::set_blink_delay(int value_in_millis) {
  blink_delay = value_in_millis;
}

void Leds::blink(const rgb_color &color, int leds) {
  for (int idx = 0; idx < 2; idx++) {
    set_all_raw(LedColors::black, LED_COUNT);
    delay(blink_delay / 4);
    set_all_raw(color, leds);
    delay(blink_delay);
  }
  dirty = true;
  publish();
}