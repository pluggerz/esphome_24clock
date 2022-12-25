#pragma once

#include "stub.h"

#include <APA102.h>

#define LEDS_ERROR_MISSING_APA102_USE_FAST_GPIO 1
#define LEDS_ERROR_NO_INTERRUPT 2

class LedColors
{
public:
    static rgb_color purple;
    static rgb_color orange;
    static rgb_color blue;
    static rgb_color red;
    static rgb_color green;
    static rgb_color black;
};

namespace Leds
{
    void setup();

    void blink(const rgb_color &color, int leds = LED_COUNT);

    void error(int leds);

    void set(uint8_t led, const rgb_color &color);
    void set_all(const rgb_color &color);

    void set_ex(uint8_t led, const rgb_color &color);
    void set_all_ex(const rgb_color &color);

    void publish();
};

#define LED_SYNC_IN 2
#define LED_ONEWIRE 3
#define LED_SYNC_OUT 4