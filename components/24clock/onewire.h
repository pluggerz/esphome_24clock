#ifndef ONEWIRE_H
#define ONEWIRE_H

#include "Arduino.h"

namespace OneWireProtocol
{
    void write(int value);
};

template <uint8_t inPin_, uint8_t outPin_>
class OneWire
{
private:
    bool changed, read_, written_;

    bool read()
    {
#ifdef USE_FAST_GPIO
        bool ret = FastGPIO::Pin<inPin_>::isInputHigh();
#else
        bool ret = digitalRead(inPin_) == HIGH;
#endif
        if (read_ != ret)
            changed = true;
        read_ = ret;
        return ret;
    }

public:
    OneWire() : changed(true), read_(false), written_(false) {}

    void setup()
    {
        pinMode(inPin_, INPUT);
        pinMode(outPin_, OUTPUT);

        read();
        set(HIGH);
    }

    bool pending()
    {
        if (changed)
            return true;
        read();
        return changed;
    }

    bool flush()
    {
        changed = false;
        return read_;
    }

    void set(const bool state)
    {
        written_ = state;
#ifdef USE_FAST_GPIO
        FastGPIO::Pin<outPin_>::setOutputValue(state);
#else
        digitalWrite(outPin_, state ? HIGH : LOW);
#endif
    }
};

#endif