#ifndef PINIO_H
#define PINIO_H

#include "stub.h"

#ifndef ESP8266
#include <FastGPIO.h>
#define USE_FAST_GPIO
#endif

#ifdef ESP8266
#define PIN_MODE "direct read/digital write"
// credits to: https://github.com/PaulStoffregen/Encoder/
#define IO_REG_TYPE uint32_t
#define PIN_TO_BASEREG(pin) ((volatile uint32_t *)(0x60000000 + (0x318)))
#define PIN_TO_BITMASK(pin) (digitalPinToBitMask(pin))
#define DIRECT_PIN_READ(base, mask) (((*(base)) & (mask)) ? 1 : 0)

#define PIN_WRITE(PIN, state) \
    digitalWrite(PIN, (state) ? HIGH : LOW);

#define PIN_READ(PIN) \
    DIRECT_PIN_READ(PIN_TO_BASEREG(PIN), PIN_TO_BITMASK(PIN))
#else
#ifdef USE_FAST_GPIO
#define PIN_MODE = "FastGPIO"

#define PIN_WRITE(PIN, state) \
    FastGPIO::Pin<PIN>::setOutputValue(state);

#define PIN_READ(PIN) \
    FastGPIO::Pin<PIN>::isInputHigh()
#else
#define PIN_MODE = "digital read/digital write"
#warning "Using direct read/digital write, make sure that is correct (it will work though)"

#define PIN_WRITE(PIN, state) \
    digitalWrite((PIN), (state) ? HIGH : LOW);

#define PIN_READ(PIN) \
    digitalRead(PIN) == HIGH

#endif // USE_FAST_GPIO
#endif // ESP8266

#endif