#include "stub.h"

#include <APA102.h>

namespace Leds
{
    void setup();

    void set(uint8_t led, const rgb_color &color);
    void set_all(const rgb_color &color);

    void publish();

    void blink();
};