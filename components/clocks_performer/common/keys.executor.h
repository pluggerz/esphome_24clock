#pragma once

#include "../clocks_shared/channel.interop.h"
#include "common/stepper.h"

class StepExecutors {
  bool stopped = true;

 public:
  static void setup(Stepper0 &stepper0, Stepper1 &stepper1);
  static void reset();
  /**
   * @brief Active or not
   *
   * @return true it at least one key is still being executed
   * @return false
   */
  void kill();
  bool active();
  /**
   * @brief will inject stop keys
   *
   */
  void request_stop();
  void send_positions();

  //
  void process_begin_keys(const channel::Message *msg);
  void process_add_keys(const channel::messages::UartKeysMessage *msg);
  void process_end_keys(int stepper_id,
                        const channel::messages::UartEndKeysMessage *msg);

  // will execute the steps
  void loop(Micros now);
} extern step_executors;
