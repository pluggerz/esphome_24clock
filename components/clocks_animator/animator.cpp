#include "animator.h"

#include <set>

#include "../clocks_director/director.h"
#include "../clocks_shared/channel.h"
#include "../clocks_shared/channel.interop.h"
#include "handles.h"
#include "helpers.h"
#include "transmitter.h"

const char *const TAG = "animator";

using animator24::ClocksAnimator;
using animator24::DistanceCalculators;
using animator24::HandlesAnimationEnum;
using animator24::HandlesAnimations;
using animator24::HandlesDistanceEnum;
using animator24::InBetweenAnimationEnum;
using animator24::InBetweenAnimations;
using channel::Message;
using channel::MsgEnum;
using channel::messages::UartEndKeysMessage;
using channel::messages::UartKeysMessage;
using handles::ClockCharacters;
using handles::ClockUtil;
using keys::cmdSpeedUtil;
using keys::CmdSpeedUtil;
using keys::DeflatedCmdKey;
using keys::InflatedCmdKey;
using rs485::BufferChannel;

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

DistanceCalculators::Func selectDistanceCalculator(
    const HandlesDistanceEnum &value) {
  switch (value) {
#define CASE(WHAT, FUNC)          \
  case HandlesDistanceEnum::WHAT: \
    return DistanceCalculators::FUNC;

    CASE(SHORTEST, shortest)
    CASE(RIGHT, clockwise)
    CASE(LEFT, antiClockwise)
    default:
      CASE(RANDOM, random())
#undef CASE
  }
}

InBetweenAnimations::Func selectInBetweenAnimation(
    const InBetweenAnimationEnum &value) {
  switch (value) {
#define CASE(WHAT, FUNC)             \
  case InBetweenAnimationEnum::WHAT: \
    return InBetweenAnimations::FUNC;

    CASE(Random, instructRandom)
    CASE(Star, instructStarAnimation)
    CASE(Dash, instructDashAnimation)
    CASE(Middle1, instructMiddlePointAnimation)
    CASE(Middle2, instructAllInnerPointAnimation)
    CASE(PacMan, instructPacManAnimation)
    default:
      CASE(None, instructNone)
#undef CASE
  }
}

HandlesAnimations::Func selectFinalAnimator(const HandlesAnimationEnum &value) {
  switch (value) {
#define CASE(WHAT, FUNC)           \
  case HandlesAnimationEnum::WHAT: \
    return HandlesAnimations::FUNC;

    CASE(Swipe, instruct_using_swipe)
    CASE(Distance, instructUsingStepCalculator)
    default:
      CASE(Random, instruct_using_random)
#undef CASE
  }
}

void copyTo(const ClockCharacters &chars, HandlesState &state) {
  auto lambda = [&state](int handleId, int hours) {
    int goal = static_cast<int>(
        NUMBER_OF_STEPS *
        static_cast<double>(hours == 13 ? 7.5 : (double)hours) / 12.0);
    state.set_ticks(handleId, goal);
    if (hours == 13) {
      state.visibilityFlags.hide(handleId);
    }
  };
  ClockUtil::iterate_handles(chars, lambda);
  for (int handleId = 0; handleId < MAX_HANDLES - 1; handleId++) {
    if (state[handleId] == state[handleId + 1])
      state.nonOverlappingFlags.hide(handleId);
  }
  // state.debug();
}

struct AnimationSettings {
  InBetweenAnimationEnum in_between_mode = InBetweenAnimationEnum::Random;
  HandlesAnimationEnum handles_mode = HandlesAnimationEnum::Random;
  HandlesDistanceEnum distance_mode = HandlesDistanceEnum::RANDOM;

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

void send_text(BufferChannel *channel, AnimationController *controller,
               const Text &text) {
  AnimationSettings settings;
  ESP_LOGI(TAG, "do_track_time -> follow up: [%c %c %c %c]", text.ch0, text.ch1,
           text.ch2, text.ch3);

  // get characters
  auto clockChars = ClockUtil::retrieveClockCharactersfromCharacters(
      text.ch0, text.ch1, text.ch2, text.ch3);

  HandlesState goal;
  copyTo(clockChars, goal);
  bool act_as_second_handle = true;

  Instructions instructions(controller);
  if (act_as_second_handle)
    instructions.iterate_handle_ids([&](int handle_id) {
      if (!goal.visibilityFlags[handle_id])
        // oke move to 12:00
        goal.set_ticks(handle_id, 0);
    });

  // final animation

  // inbetween
  auto inBetweenAnimation =
      selectInBetweenAnimation(settings.get_in_between_animation());
  auto distanceCalculator =
      selectDistanceCalculator(settings.get_handles_distance_mode());
  auto finalAnimator =
      selectFinalAnimator(settings.get_handles_animation_mode());

  auto speed = settings.get_speed();

  inBetweenAnimation(instructions, speed);
  finalAnimator(instructions, speed, goal, distanceCalculator);

  // lets wait for all...
  InBetweenAnimations::instructDelayUntilAllAreReady(instructions, 32);
  float millis_left = text.millis_left;
  ESP_LOGI(TAG, "millis_left: %f", millis_left);
  if (act_as_second_handle)
    instructions.iterate_handle_ids([&](int handle_id) {
      if (!goal.visibilityFlags[handle_id])
        instructions.follow_seconds(handle_id, true);
    });

  transmitter::Transmitter(controller, channel)
      .sendInstructions(instructions, millis_left);
}

void ClocksAnimator::request_time_change(int hours, int minutes) {
  ESP_LOGW(TAG, "request_positions %d:%d", hours, minutes);
  AnimationSettings settings;

  send_text(director->get_channel(), director->get_animation_controller(),
            Text::from_time(hours, minutes));
}