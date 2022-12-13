#include "stub.h"

#include "onewire.h"
#include "pins.h"
#include "leds.h"

OneWire<SYNC_IN_PIN, SYNC_OUT_PIN> wire;

void setup()
{
    Leds::setup();
    wire.setup();
}

void loop()
{
    if (wire.pending())
    {
        auto state = wire.flush();
        auto color = state ? rgb_color(0xFF, 0, 0) : rgb_color(0x00, 0xFF, 0);
        Leds::set(0, color);
        Leds::publish();
        wire.set(state);
    }
}
