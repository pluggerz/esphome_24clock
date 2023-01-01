#pragma once

#include <FastGPIO.h>

#include "Arduino.h"
#define APA102_USE_FAST_GPIO

#define LED_COUNT 12

#define STEP_MULTIPLIER 4
#define NUMBER_OF_STEPS (720 * STEP_MULTIPLIER)

#include "onewire.interop.h"

void transmit(const onewire::OneCommand &value);