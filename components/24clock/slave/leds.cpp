#include "leds.h"
#include "pins.h"

rgb_color leds[LED_COUNT];
bool dirty = true;

rgb_color LedColors::purple = rgb_color(0xFF, 0x00, 0xFF);
rgb_color LedColors::orange = rgb_color(0xFF, 0xFF, 0x00);
rgb_color LedColors::blue = rgb_color(0x00, 0x00, 0xFF);
rgb_color LedColors::red = rgb_color(0xFF, 0x00, 0x00);
rgb_color LedColors::green = rgb_color(0x00, 0xFF, 0x00);
rgb_color LedColors::black = rgb_color(0x00, 0x00, 0x00);

// Create an object for writing to the LED strip.
extern APA102<LED_DATA_PIN, LED_CLOCK_PIN> ledStrip;

void Leds::setup()
{
    blink(rgb_color(0xFF, 0x00, 0x00));
    blink(rgb_color(0x00, 0xFF, 0x00));
    blink(rgb_color(0x00, 0x00, 0xFF));

    Leds::set_all_ex(rgb_color(0x00, 0x00, 0x00));
    Leds::publish();
}

void Leds::error(int leds)
{
    while (true)
        blink(LedColors::red, leds);
}

void Leds::set_all(const rgb_color &color)
{
}

void Leds::set(uint8_t led, const rgb_color &color)
{
}

void Leds::set_all_ex(const rgb_color &color)
{
    dirty = true;
    for (auto idx = 0; idx < LED_COUNT; ++idx)
    {
        leds[idx] = color;
    }
}

void Leds::set_ex(uint8_t led, const rgb_color &color)
{
    led = (led - 1) % LED_COUNT;
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

void set_all_raw(const rgb_color &color, int leds)
{
    ledStrip.startFrame();
    const auto brightness = 31;
    for (uint8_t idx = 1; idx < LED_COUNT; idx++)
    {
        ledStrip.sendColor(idx < leds ? color : LedColors::black, brightness);
    }
    ledStrip.sendColor(1 <= leds ? color : LedColors::black, brightness);
    ledStrip.endFrame(LED_COUNT);
}

void Leds::blink(const rgb_color &color, int leds)
{
    for (int idx = 0; idx < 2; idx++)
    {
        set_all_raw(LedColors::black, LED_COUNT);
        delay(50);
        set_all_raw(color, leds);
        delay(200);
    }
    dirty = true;
    publish();
}