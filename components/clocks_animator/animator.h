#include "../clocks_shared/stub.h"
#include "esphome/core/component.h"

namespace clock24 {
class Director;
}

namespace animator24 {
const char *const TAG = "animator";

namespace in_between {
enum Enum {
  Random,
  None,
  Star,
  Dash,
  Middle1,
  Middle2,
  PacMan,
};
}

namespace handles_distance {
enum Enum {
  RANDOM,
  SHORTEST,
  LEFT,
  RIGHT,
};
}

namespace handles_animation {
enum Enum {
  Random,
  Swipe,
  Distance,
};
}

class ClocksAnimator : public esphome::Component {
 public:
  clock24::Director *director = nullptr;
  handles_distance::Enum handles_distance_mode = handles_distance::RANDOM;
  handles_animation::Enum handles_animation_mode = handles_animation::Random;
  in_between::Enum in_between_mode = in_between::Random;
  uint32_t in_between_flags = 0xFFFFFFFF;
  uint32_t distance_flags = 0xFFFFFFFF;
  uint32_t handles_animation_flags = 0xFFFFFFFF;

  virtual void dump_config() override {
    LOGI(TAG, "Animator:");
    LOGI(TAG, " in_between_flags: %H", in_between_flags);
    LOGI(TAG, " distance_flags:   %H", distance_flags);
    LOGI(TAG, " handles_flags:    %H", handles_animation_flags);
  }

#define CODE(TYPE, flags)                                            \
  bool state(const TYPE &mode) const { return flags & (1 << mode); } \
  void turn_on(const TYPE mode) {                                    \
    flags |= (1 << mode);                                            \
    LOGI(TAG, #flags ": %d", flags);                                 \
  }                                                                  \
  void turn_off(const TYPE mode) {                                   \
    flags &= ~(1 << mode);                                           \
    LOGI(TAG, #flags ": %d", flags);                                 \
  }

  CODE(in_between::Enum, in_between_flags);
  CODE(handles_distance::Enum, distance_flags);
  CODE(handles_animation::Enum, handles_animation_flags);

  void set_handles_distance_mode(handles_distance::Enum mode) {
    handles_distance_mode = mode;
  }

  void request_time_change(int hours, int minutes);

  void set_director(clock24::Director *director) { this->director = director; }
};  // namespace animator24
}  // namespace animator24