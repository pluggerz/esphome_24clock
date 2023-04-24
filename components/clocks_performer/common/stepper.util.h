#include "../clocks_shared/pins.h"
#include "Arduino.h"
#include "stepper.h"

class StepperUtil {
 public:
  static void setup_steppers() {
    // set mode
    pinMode(MOTOR_SLEEP_PIN, OUTPUT);
    pinMode(MOTOR_ENABLE, OUTPUT);
    pinMode(MOTOR_RESET, OUTPUT);

    stepper0.setup();
    stepper1.setup();

    delay(10);

    // set state
    digitalWrite(MOTOR_SLEEP_PIN, LOW);
    digitalWrite(MOTOR_ENABLE, HIGH);
    digitalWrite(MOTOR_RESET, LOW);

    // wait at least 1ms
    delay(10);
    digitalWrite(MOTOR_SLEEP_PIN, HIGH);
    digitalWrite(MOTOR_ENABLE, LOW);
    digitalWrite(MOTOR_RESET, HIGH);

    // wait at least 200ns
    delay(10);
  }

  static void calibrate_steppers() {
    int speed = 10;
    stepper0.set_speed_in_revs_per_minute(speed);
    stepper1.set_speed_in_revs_per_minute(speed);
    while (!stepper0.find_magnet_tick() || !stepper1.find_magnet_tick()) {
    }
    for (int idx = 0; idx < 240; ++idx) {
      stepper0.step(-1);
      stepper1.step(-1);
    }

    stepper0.set_speed_in_revs_per_minute(speed);
    stepper1.set_speed_in_revs_per_minute(speed);
    while (!stepper0.find_magnet_tick() || !stepper1.find_magnet_tick()) {
    }
    for (int idx = 0; idx < 240; ++idx) {
      stepper0.step(-1);
      stepper1.step(-1);
    }
  }
};