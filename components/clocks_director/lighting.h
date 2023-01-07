namespace lighting {
enum LightingMode {
  // NOTE, do not change the ids below!
  WarmWhiteShimmer = 0,
  RandomColorWalk = 1,
  TraditionalColors = 2,
  ColorExplosion = 3,
  Gradient = 4,
  BrightTwinkle = 5,
  Collision = 6,
  // NOTE, do not change the ids above!
  Off = 7,
  Solid = 8,
  Rainbow = 9,
  Debug = 10,
};

#if defined(IS_DIRECTOR)
#include "../clocks_shared/channel.interop.h"
#include "director.h"

const char *const TAG = "lighting";

using clock24::Director;

class Controller {
  Director *director = nullptr;

 public:
  void set_director(Director *director) { this->director = director; }
  int red = 0, green = 0, blue = 0xFF;

  void use_mode(LightingMode mode) {
    if (this->director == nullptr) {
      LOGE(TAG, "director not assigned!?");
      return;
    }
    director->get_channel()->send(
        channel::messages::LightingMode(mode, red, green, blue));
  }

  void switch_to_solid() { use_mode(LightingMode::Solid); }

  void set_solid_red(float state) {
    LOGD(TAG, "set_solid_red: %f", state);
    red = 0xFF * state;
    switch_to_solid();
  }
  void set_solid_green(float state) {
    LOGD(TAG, "set_solid_green: %f", state);
    green = 0xFF * state;
    switch_to_solid();
  }
  void set_solid_blue(float state) {
    LOGD(TAG, "set_solid_blue: %f", state);
    blue = 0xFF * state;
    switch_to_solid();
  }
};
#endif
}  // namespace lighting
