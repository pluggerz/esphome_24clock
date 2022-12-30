#pragma once

#include "stub.h"

struct Ticks {
  static int normalize(int value) {
    while (value < 0) value += NUMBER_OF_STEPS;
    while (value >= NUMBER_OF_STEPS) value -= NUMBER_OF_STEPS;
    return value;
  }
};