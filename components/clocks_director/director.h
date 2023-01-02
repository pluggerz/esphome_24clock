#include "stub.h"
#if defined(MASTER) && !defined(MASTER_H)
#define MASTER_H

#include "channel.h"
#include "esphome/components/time/real_time_clock.h"
#include "esphome/core/component.h"
#include "onewire.h"
#include "onewire.interop.h"

class AnimationController;

namespace clock24 {
class Stepper {
 public:
  int offset = 0;
  int ticks = -1;
};

class Performer {
 public:
  int animator_id = -1;
  Stepper stepper0, stepper1;

  void set_animator_id(int value) { this->animator_id = value; }
  void set_magnet_offsets(int _offset0, int _offset1) {
    this->stepper0.offset = _offset0;
    this->stepper1.offset = _offset1;
  }
};

class Director : public esphome::Component {
 private:
  bool dumped = false;
  int _performers = -1;
  bool _killed = false;
  Performer performers[NMBR_OF_PERFORMERS];
  int baudrate = 0;

 protected:
  AnimationController *animation_controller_ = nullptr;

  virtual void setup() override;
  virtual void loop() override;

 public:
  Director();

  Performer &performer(int idx) { return performers[idx]; }

  void kill();
  virtual void dump_config() override;

  void set_baudrate(int value) { baudrate = value; }

  void request_positions();

  rs485::BufferChannel *get_channel();
  AnimationController *get_animation_controller() {
    return animation_controller_;
  }
};

class Animator : public esphome::Component {};
}  // namespace clock24
#endif