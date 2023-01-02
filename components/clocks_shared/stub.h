#pragma once

#define MODE_ONEWIRE_PASSTROUGH 1
#define MODE_ONEWIRE_MIRROR 2
#define MODE_ONEWIRE_VALUE 3

#define MODE_ONEWIRE_CMD 4

// make sure all slaves+master use this one as well
#define MODE_CHANNEL 6

// MODE_ONEWIRE_INTERACT can work with PASSTHROUGH and MIRROR for slaves
#define MODE_ONEWIRE_INTERACT 7

#define MODE MODE_ONEWIRE_INTERACT

typedef unsigned long Micros;
typedef unsigned long Millis;

constexpr int NMBR_OF_PERFORMERS = 24;

constexpr int MAX_HANDLES = 48;

#ifdef ESP8266
#define MASTER
#define MASTER

#include "../clocks_director/stub.master.h"

#else
#define SLAVE
#define PERFORMER

#include "slave/stub.slave.h"

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