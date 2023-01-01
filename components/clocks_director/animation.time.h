#include "stub.h"

class AnimationController;
namespace rs485 {
class BufferChannel;
};

struct Text {
  char ch0 = '*', ch1 = '*', ch2 = '*', ch3 = '*';
  long millis_left = 0L;

  bool __equal__(const Text &t) const {
    return ch0 == t.ch0 && ch1 == t.ch1 && ch2 == t.ch2 && ch3 == t.ch3;
  }

  static Text from_time(uint8_t hour, uint8_t minute) {
    Text text;
    text.ch0 = '0' + (hour / 10);
    text.ch1 = '0' + (hour % 10);
    text.ch2 = '0' + (minute / 10);
    text.ch3 = '0' + (minute % 10);
    return text;
  };
};

namespace animation {
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
  Random,
  Shortest,
  Left,
  Right,
};

enum class HandlesAnimationEnum {
  Random,
  Swipe,
  Distance,
};

struct AnimationSettings {
  InBetweenAnimationEnum in_between_mode = InBetweenAnimationEnum::Random;
  HandlesAnimationEnum handles_mode = HandlesAnimationEnum::Random;
  HandlesDistanceEnum distance_mode = HandlesDistanceEnum::Random;

  int get_speed() const { return 12; }

  void set_handles_distance_mode(HandlesDistanceEnum value) {
    distance_mode = value;
  }
  HandlesDistanceEnum get_handles_distance_mode() const {
    return distance_mode;
  }

  void set_handles_animation_mode(HandlesAnimationEnum value) {
    handles_mode = value;
  }
  HandlesAnimationEnum get_handles_animation_mode() const {
    return handles_mode;
  }

  void set_in_between_animation_mode(InBetweenAnimationEnum value) {
    in_between_mode = value;
  }
  InBetweenAnimationEnum get_in_between_animation() const {
    return in_between_mode;
  }
};

class AnimationRequest {};

class TrackTimeRequest : public AnimationRequest {};
}  // namespace animation

// TODO: refactor
void send_text(rs485::BufferChannel *channel, AnimationController *controller,
               const animation::AnimationSettings &settings, const Text &text);