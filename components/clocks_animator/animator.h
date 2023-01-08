#include "../clocks_shared/stub.h"
#include "esphome/core/component.h"

namespace clock24 {
class Director;
}

namespace animator24 {
const char *const TAG = "animator";

// NOTE: Random should be the last one
namespace in_between {
enum Enum { None, Star, Dash, Middle1, Middle2, PacMan, RANDOM };
}
// NOTE: Random should be the last one
namespace handles_distance {
enum Enum { SHORTEST, LEFT, RIGHT, RANDOM };
}
// NOTE: Random should be the last one
namespace handles_animation {
enum Enum { Swipe, Distance, TERMINATOR, RANDOM };
}

class AnimationSettings {
 private:
  template <class E>
  E pick_random(uint32_t flags, E random_enum) const {
    uint32_t mask = (1 << random_enum) - 1;
    uint32_t masked_flags = mask & flags;
    if (masked_flags == 0) {
      LOGI(TAG, "pick_random: 0");
      return static_cast<E>(0);
    }
    while (true) {
      auto bit = ::random(random_enum - 1);
      LOGI(TAG, "pick_random: %d ?", bit);
      if (masked_flags & (1 << bit)) {
        LOGI(TAG, "pick_random: %d !", bit);
        return static_cast<E>(bit);
      }
    }
    return static_cast<E>(0);
  }

 public:
  handles_animation::Enum handles_animation_mode = handles_animation::RANDOM;
  handles_distance::Enum handles_distance_mode = handles_distance::RANDOM;
  in_between::Enum in_between_mode = in_between::RANDOM;
  uint32_t handles_animation_flags = (1 << handles_animation::RANDOM) - 1;
  uint32_t handles_distance_flags = (1 << handles_distance::RANDOM) - 1;
  uint32_t in_between_flags = (1 << in_between::RANDOM) - 1;

  handles_animation::Enum pick_handles_animation() const {
    auto ret = handles_animation_mode;
    if (ret >= handles_animation::RANDOM)
      ret = pick_random(handles_animation_flags, handles_animation::RANDOM);
    LOGI(TAG, "pick_handles_animation: %d -> %d", handles_animation_mode, ret);
    return ret;
  }

  handles_distance::Enum pick_handles_distance() const {
    auto ret = handles_distance_mode;
    if (ret >= handles_distance::RANDOM)
      ret = pick_random(handles_distance_flags, handles_distance::RANDOM);
    LOGI(TAG, "pick_handles_distance: %d -> %d", handles_distance_mode, ret);
    return ret;
  }

  in_between::Enum pick_in_between() const {
    auto ret = in_between_mode;
    if (in_between_mode >= in_between::RANDOM)
      ret = pick_random(in_between_flags, in_between::RANDOM);
    LOGI(TAG, "pick_in_between: %d -> %d", in_between_mode, ret);
    return ret;
  }

  int get_speed() const { return 12; }
};

class ClocksAnimator : public esphome::Component, public AnimationSettings {
 public:
  clock24::Director *director = nullptr;

  virtual void dump_config() override {
    LOGI(TAG, "Animator:");
    LOGI(TAG, " in_between_flags: %H", in_between_flags);
    LOGI(TAG, " distance_flags:   %H", handles_distance_flags);
    LOGI(TAG, " handles_flags:    %H", handles_animation_flags);
  }

#define CODE(TYPE, flags, mode)                                      \
  bool state(const TYPE &mode) const { return flags & (1 << mode); } \
  void turn_on(const TYPE &mode) {                                   \
    flags |= (1 << mode);                                            \
    LOGI(TAG, #flags "/turn_on: %d", flags);                         \
  }                                                                  \
  void turn_off(const TYPE &mode) {                                  \
    flags &= ~(1 << mode);                                           \
    LOGI(TAG, #flags "/turn_off: %d", flags);                        \
  }                                                                  \
  void pick(const TYPE &mode) {                                      \
    this->mode = mode;                                               \
    LOGI(TAG, #flags #mode ": %d", mode);                            \
  }

  CODE(in_between::Enum, in_between_flags, in_between_mode);
  CODE(handles_distance::Enum, handles_distance_flags, handles_distance_mode);
  CODE(handles_animation::Enum, handles_animation_flags,
       handles_animation_mode);

  void set_handles_distance_mode(handles_distance::Enum mode) {
    handles_distance_mode = mode;
  }

  void request_time_change(int hours, int minutes);

  void set_director(clock24::Director *director) { this->director = director; }
};  // namespace animator24
}  // namespace animator24