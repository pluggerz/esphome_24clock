#pragma once

#include "Arduino.h"

#include <FastGPIO.h>
#define APA102_USE_FAST_GPIO

#define LED_COUNT 12

#define STEP_MULTIPLIER 4
#define NUMBER_OF_STEPS (720 * STEP_MULTIPLIER)
