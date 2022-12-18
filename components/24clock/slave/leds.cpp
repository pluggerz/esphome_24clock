#include "leds.h"
#include "pins.h"

rgb_color leds[LED_COUNT];
bool dirty = true;

// Create an object for writing to the LED strip.
extern APA102<LED_DATA_PIN, LED_CLOCK_PIN> ledStrip;

void Leds::setup()
{
    Leds::set_all(rgb_color(0xFF, 0x00, 0x00));
    Leds::publish();
    delay(500);
    Leds::set_all(rgb_color(0x00, 0xFF, 0x00));
    Leds::publish();
    delay(500);
    Leds::set_all(rgb_color(0x00, 0x00, 0xFF));
    Leds::publish();
    delay(500);
    Leds::set_all(rgb_color(0xFF, 0x00, 0x00));
    Leds::publish();
}

void Leds::set_all(const rgb_color &color)
{
    dirty = true;
    for (auto idx = 0; idx < LED_COUNT; ++idx)
    {
        leds[idx] = color;
    }
}

void Leds::set(uint8_t led, const rgb_color &color)
{
    dirty = true;
    leds[led] = color;
}

void Leds::publish()
{
    if (!dirty)
        return;

    dirty = false;
    ledStrip.startFrame();
    const auto brightness = 31;
    for (uint8_t idx = 0; idx < LED_COUNT; idx++)
    {
        ledStrip.sendColor(leds[idx], brightness);
    }
    ledStrip.endFrame(LED_COUNT);
}

void Leds::blink()
{
}