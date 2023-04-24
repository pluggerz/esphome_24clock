#pragma once

#include "../clocks_shared/shared.types.h"

constexpr int NMBR_OF_PERFORMERS = 24;

constexpr int MAX_HANDLES = 48;

#ifdef ESP8266
#define MASTER

#define UART_BAUDRATE 115200
// 57600 // 256000  // 115200  // 28800  // 115200  // 9600

#include "Arduino.h"

#define NUMBER_OF_STEPS 720

#else
#define SLAVE
#define PERFORMER

// #include "slave/stub.slave.h"

#include <FastGPIO.h>

#include "Arduino.h"
#define APA102_USE_FAST_GPIO

#define LED_COUNT 12

#define STEP_MULTIPLIER 4
#define NUMBER_OF_STEPS (720 * STEP_MULTIPLIER)

#endif

#ifdef ESP8266
#define IS_DIRECTOR
#else
#define IS_PERFORMER
#endif

class Action {
 protected:
  Micros t0;

 public:
  virtual void setup() { t0 = millis(); };
  virtual void loop(){};
  virtual void final(){};
};

class IntervalAction : public Action {
  Micros _last = 0, _delay;

 protected:
  virtual void first_update() { update(); }

  virtual void update() = 0;
  IntervalAction(Micros delay = 2000) { _delay = delay; }

 public:
  void setup() { _last = 0; }

  void set_delay_in_millis(Millis millis) { this->_delay = millis; }

  void loop() override {
    if (millis() - _last > _delay) {
      if (_last == 0)
        first_update();
      else
        update();
      _last = millis();
    }
  }
};
