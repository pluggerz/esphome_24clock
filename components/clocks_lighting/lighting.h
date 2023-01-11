#pragma once

#if defined(ESP8266)
#include "../clocks_director/director.h"
#include "../clocks_shared/channel.interop.h"
#include "../clocks_shared/lighting.h"

namespace lighting {
const char *const TAG = "lighting";

using clock24::AttachListener;
using clock24::Director;

class LightingController : public esphome::Component, public AttachListener {
 protected:
  Director *director = nullptr;
  LightingMode mode = LightingMode::Debug;

  virtual void on_attach() override { use_mode(this->mode); }

  /***
   * If solid is selected, it will update the color
   */
  void update_solid() {
    if (mode == LightingMode::Solid) use_mode(LightingMode::Solid);
  }

 public:
  virtual void dump_config() override {
    LOGI(TAG, "lighting::Controller:");
    LOGI(TAG, "   mode: %d", mode);
  }
  void set_director(Director *director);
  int red = 0, green = 0, blue = 0xFF, brightness = 31;

  void pick(LightingMode mode) { use_mode(mode); }

  void use_mode(LightingMode mode) {
    if (this->director == nullptr) {
      LOGE(TAG, "director not assigned!?");
      return;
    }
    LOGI(TAG, "LightingMode: %d", mode);
    this->mode = mode;
    this->director->get_channel()->send(
        channel::messages::LightingMode(mode, red, green, blue, brightness));
  }

  void set_solid_red(float state) {
    LOGD(TAG, "set_solid_red: %f", state);
    this->red = 0xFF * state;
    update_solid();
  }
  void set_solid_green(float state) {
    LOGD(TAG, "set_solid_green: %f", state);
    this->green = 0xFF * state;
    update_solid();
  }
  void set_solid_blue(float state) {
    LOGD(TAG, "set_solid_blue: %f", state);
    this->blue = 0xFF * state;
    update_solid();
  }
  void set_brightness(float state) {
    LOGD(TAG, "set_brightness: %f", state);
    this->brightness = 31 * state;
    use_mode(this->mode);
  }
};
#endif
}  // namespace lighting
