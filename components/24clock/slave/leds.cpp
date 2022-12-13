#include "leds.h"
#include "pins.h"

rgb_color leds[LED_COUNT];

// Create an object for writing to the LED strip.
extern APA102<LED_DATA_PIN, LED_CLOCK_PIN> ledStrip;

void Leds::setup()
{
    for (auto idx = 0; idx < LED_COUNT; ++idx)
    {
        leds[idx] = rgb_color(0xFF, 0xFF, 0xFF);
    }
    Leds::publish();
}

void Leds::set(uint8_t led, const rgb_color &color)
{
    leds[led] = color;
}

void Leds::publish()
{
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