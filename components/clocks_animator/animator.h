#include "../clocks_shared/log.h"
#include "../clocks_shared/stub.h"
#include "esphome/core/component.h"

namespace clock24 {
class Director;
}

namespace animator24 {
const char *const TAG = "animator";
using clock24::Director;

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
enum Enum { Swipe, Distance, RANDOM };
}

class AnimationSettings {
 private:
  template <class E>
  E pick_random(uint32_t flags, E random_enum) const {
    uint32_t mask = (1 << random_enum) - 1;
    uint32_t masked_flags = mask & flags;
    LOGI(TAG, "pick_random: flags=% mask=%d, masked_flags=%d max_enum=%d",
         flags, mask, masked_flags, random_enum);

    if (masked_flags == 0) {
      LOGI(TAG, "pick_random: 0");
      return static_cast<E>(0);
    }
    while (true) {
      auto bit = ::random(random_enum);
      LOGI(TAG, "pick_random: %d? max: %d ?", bit, random_enum);
      if (masked_flags & (1 << bit)) {
        LOGI(TAG, "pick_random: %d! max %d", bit, random_enum);
        return static_cast<E>(bit);
      }
    }
  }

 public:
#define PICK_CODE(ENUM)                                                     \
  ENUM::Enum ENUM##_mode = ENUM::RANDOM;                                    \
  uint32_t ENUM##_flags = (1 << ENUM::RANDOM) - 1;                          \
                                                                            \
  ENUM::Enum pick_##ENUM() const {                                          \
    auto ret = ENUM##_mode;                                                 \
    if (ret >= ENUM::RANDOM) ret = pick_random(ENUM##_flags, ENUM::RANDOM); \
    LOGI(TAG, #ENUM " pick: %d -> %d", ENUM##_mode, ret);                   \
    return ret;                                                             \
  }                                                                         \
  bool state(const ENUM::Enum &mode) const {                                \
    return ENUM##_flags & (1 << mode);                                      \
  }                                                                         \
  void turn_on(const ENUM::Enum &mode) {                                    \
    ENUM##_flags |= (1 << mode);                                            \
    LOGI(TAG, #ENUM "/turn_on: %d / %d", mode, ENUM##_flags);               \
  }                                                                         \
  void turn_off(const ENUM::Enum &mode) {                                   \
    ENUM##_flags &= ~(1 << mode);                                           \
    LOGI(TAG, #ENUM "/turn_off: %d / %d", mode, ENUM##_flags);              \
  }                                                                         \
  void pick(const ENUM::Enum &mode) {                                       \
    this->ENUM##_mode = mode;                                               \
    LOGI(TAG, #ENUM ": %d", mode);                                          \
  }
  PICK_CODE(handles_animation)
  PICK_CODE(handles_distance)
  PICK_CODE(in_between)
#undef PICK_CODE

  int get_speed() const { return 12; }
};

class ClocksAnimator : public esphome::Component, public AnimationSettings {
 public:
  clock24::Director *director = nullptr;

  virtual void dump_config() override {
    LOGI(TAG, "Animator:");
    LOGI(TAG, " in_between(_flags): %d / %H", in_between_mode,
         in_between_flags);
    LOGI(TAG, " distance(_flags):   %d / %H", handles_distance_mode,
         handles_distance_flags);
    LOGI(TAG, " handles(_flags):    %d / %H", handles_animation_mode,
         handles_animation_flags);
  }

  void set_handles_distance_mode(handles_distance::Enum mode) {
    handles_distance_mode = mode;
  }

  void request_time_change(int hours, int minutes);
  void request_test_pattern_change(char ch);

  void set_director(clock24::Director *director) { this->director = director; }
};  // namespace animator24
}  // namespace animator24