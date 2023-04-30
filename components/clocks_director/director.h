#pragma once

#ifdef ESP8266
#include <vector>

#include "../clocks_shared/channel.h"
#include "../clocks_shared/onewire.interop.h"
#include "esphome/components/time/real_time_clock.h"
#include "esphome/components/wifi/wifi_component.h"
#include "esphome/core/component.h"

class AnimationController;

namespace clock24 {
class Stepper {
 public:
  int16_t offset = 0;
  int16_t ticks = -1;
};
using esphome::wifi::WiFiComponent;

class Util {
 public:
  static String format_up_time(int seconds) {
    int days = seconds / (24 * 3600);
    seconds = seconds % (24 * 3600);
    int hours = seconds / 3600;
    seconds = seconds % 3600;
    int minutes = seconds / 60;
    seconds = seconds % 60;
    if (days) {
      return {(String(days) + "d " + String(hours) + "h " + String(minutes) +
               "m " + String(seconds) + "s")
                  .c_str()};
    } else if (hours) {
      return {(String(hours) + "h " + String(minutes) + "m " + String(seconds) +
               "s")
                  .c_str()};
    } else if (minutes) {
      return {(String(minutes) + "m " + String(seconds) + "s").c_str()};
    } else {
      return {(String(seconds) + "s").c_str()};
    }
  }
};

class Performer {
 public:
  int animator_id = -1;
  Stepper stepper0, stepper1;
  int channel_errors = 0;
  int channel_skips = 0;
  int one_wire_errors = 0;
  int uptime_in_seconds = 0;

  void set_animator_id(int value) { this->animator_id = value; }
  void set_magnet_offsets(int _offset0, int _offset1) {
    this->stepper0.offset = _offset0;
    this->stepper1.offset = _offset1;
  }

  String report() {
    auto p0 = stepper0.ticks;
    auto p1 = stepper1.ticks;
    return String("uptime: ") + Util::format_up_time(uptime_in_seconds) +
           String(" T:") + String(p0 / 60.0) + String(" ") + String(p1 / 60.0);
  }
};

class AttachListener {
 public:
  virtual void on_attach() = 0;
};

class Director : public esphome::Component {
 private:
  bool accepted = true;
  bool dumped = false;
  int _performers = -1;
  bool _killed = false;
  Performer performers[NMBR_OF_PERFORMERS];
  int baudrate = 0;
  std::vector<AttachListener *> listeners;
  WiFiComponent *wi_fi_component = nullptr;

 protected:
  AnimationController *animation_controller_ = nullptr;

  virtual void setup() override;
  void start();
  bool is_started();
  virtual void loop() override;

 public:
  Director();

  int get_channel_error_count() const {
    int ret = 0;
    for (int idx = 0; idx < NMBR_OF_PERFORMERS; ++idx)
      ret += performers[idx].channel_errors;
    return ret;
  }

  int get_one_wire_error_count() const {
    int ret = 0;
    for (int idx = 0; idx < NMBR_OF_PERFORMERS; ++idx)
      ret += performers[idx].one_wire_errors;
    return ret;
  }

  Performer &performer(int idx) { return performers[idx]; }

  void add_listener(AttachListener *listener) { listeners.push_back(listener); }

  void on_attach() {
    for (auto listener : listeners) listener->on_attach();
  }

  void kill();
  virtual void dump_config() override;

  void set_baudrate(int value) { this->baudrate = value; }
  void set_wi_fi_component(WiFiComponent *value) {
    this->wi_fi_component = value;
  }

  void request_positions();

  rs485::BufferChannel *get_channel();
  AnimationController *get_animation_controller() {
    return animation_controller_;
  }
};

class Animator : public esphome::Component {};
}  // namespace clock24
#endif