#include "../clocks_director/animation.h"
#include "../clocks_director/stub.h"
#include "esphome/core/component.h"

namespace clock24 {
class Director;
}

namespace animator24 {
enum class InBetweenAnimationEnum {
  Random,
  None,
  Star,
  Dash,
  Middle1,
  Middle2,
  PacMan,
};

enum class HandlesDistanceEnum {
  RANDOM,
  SHORTEST,
  LEFT,
  RIGHT,
};

enum class HandlesAnimationEnum {
  Random,
  Swipe,
  Distance,
};

class ClocksAnimator : public esphome::Component {
 public:
  clock24::Director *director = nullptr;
  HandlesDistanceEnum handles_distance_mode = HandlesDistanceEnum::RANDOM;
  HandlesAnimationEnum handles_animation = HandlesAnimationEnum::Random;
  InBetweenAnimationEnum in_between_mode = InBetweenAnimationEnum::Random;

  void set_handles_distance_mode(HandlesDistanceEnum mode) {
    handles_distance_mode = mode;
  }

  void request_time_change(int hours, int minutes);

  void set_director(clock24::Director *director) { this->director = director; }
};
}  // namespace animator24