#pragma once

#include "../clocks_shared/channel.interop.h"
#include "stepper.h"

class StepExecutors {
 public:
  static void setup(Stepper0 &stepper0, Stepper1 &stepper1);
  static void reset();
  /**
   * @brief Active or not
   *
   * @return true it at least one key is still being executed
   * @return false
   */
  static bool active();
  /**
   * @brief will inject stop keys
   *
   */
  static void request_stop();

  //
  static void process_begin_keys(const channel::Message *msg);
  static void process_add_keys(const channel::messages::UartKeysMessage *msg);
  static void process_end_keys(
      int stepper_id, const channel::messages::UartEndKeysMessage *msg);

  // will execute the steps
  static void loop(Micros now);
};
