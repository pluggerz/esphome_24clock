#include "stub.h"

#if defined(ESP8266)
#include <set>
#include <vector>

#include "animation.h"
#include "animation.time.h"
#include "channel.h"
#include "channel.interop.h"
#include "handles.h"
#include "keys.h"

using animation::HandlesAnimationEnum;
using animation::HandlesDistanceEnum;
using animation::InBetweenAnimationEnum;
using channel ::ChannelInterop;
using channel::Message;
using channel::MsgEnum;
using keys::cmdSpeedUtil;
using keys::CmdSpeedUtil;
using keys::DeflatedCmdKey;
using keys::InflatedCmdKey;
using rs485::BufferChannel;

#include "esphome/core/log.h"
using namespace esphome;

const char *const TAG = "animation.text";

#define MAX_ANIMATION_KEYS 90
#define MAX_ANIMATION_KEYS_PER_MESSAGE 14  // MAX 14!

struct UartKeysMessage : public Message {
  // it would be tempting to use: Cmd cmds[MAX_ANIMATION_KEYS_PER_MESSAGE] ={};
  // but padding is messed up between esp and uno :() note we control the bits
  // better ;P
 private:
  uint8_t _size;
  uint16_t cmds[MAX_ANIMATION_KEYS_PER_MESSAGE] = {};

 public:
  UartKeysMessage(uint8_t destination_id, uint8_t _size)
      : Message(-1, MsgEnum::MSG_SEND_KEYS, destination_id), _size(_size) {}
  uint8_t size() const { return _size; }

  void set_key(int idx, uint16_t value) { cmds[idx] = value; }

  uint16_t get_key(int idx) const { return cmds[idx]; }
} __attribute__((packed, aligned(1)));

/***
 *
 * Essentially every minute we send animation keys.
 * Some type of animations depends on the seconds.
 * Ideally, a minute has 60 seconds, but some time could be lost in prepping.
 * So, this message will give the actual minute.
 * Or at least the time when the master will probability send the next
 * instructions.
 */
struct UartEndKeysMessage : public Message {
 public:
  uint32_t number_of_millis_left;
  uint8_t turn_speed, turn_steps;
  uint8_t speed_map[8];
  uint64_t speed_detection;

  UartEndKeysMessage(const uint8_t turn_speed, const uint8_t turn_steps,
                     const uint8_t (&speed_map)[8], uint64_t _speed_detection,
                     uint32_t number_of_millis_left)
      : Message(-1, MsgEnum::MSG_END_KEYS, ChannelInterop::ALL_PERFORMERS),
        number_of_millis_left(number_of_millis_left),
        turn_speed(turn_speed),
        turn_steps(turn_steps),
        speed_detection(_speed_detection) {
    for (int idx = 0; idx < 8; ++idx) {
      this->speed_map[idx] = speed_map[idx];
    }
  }
} __attribute__((packed, aligned(1)));

void updateSpeeds(const Instructions &instructions) {
  // gather speeds
  std::set<int> new_speeds_as_set{1};
  for (const auto &cmd : instructions.cmds) {
    if (cmd.ignorable() || cmd.ghost_or_alike()) continue;
    new_speeds_as_set.insert(cmd.speed());
  }

  if (new_speeds_as_set.size() > cmdSpeedUtil.max_inflated_speed) {
    ESP_LOGE(TAG, "Array!? size=%d, id=%d", new_speeds_as_set.size(),
             cmdSpeedUtil.max_inflated_speed);
    return;
  }

  CmdSpeedUtil::Speeds speeds;
  int last_speed = 1;
  int idx = 0;
  for (auto speed : new_speeds_as_set) {
    speeds[idx++] = speed;
    last_speed = speed;
  }
  for (; idx <= cmdSpeedUtil.max_inflated_speed; ++idx) {
    speeds[idx] = last_speed;
  }
  cmdSpeedUtil.set_speeds(speeds);
}

class Transmitter {
 public:
  AnimationController *animation_controller;
  BufferChannel *channel;

  Transmitter(AnimationController *animation_controller, BufferChannel *channel)
      : animation_controller(animation_controller), channel(channel) {}

  void sendCommandsForHandle(int animatorHandleId,
                             const std::vector<DeflatedCmdKey> &commands) {
    auto physicalHandleId =
        animation_controller->mapAnimatorHandle2PhysicalHandleId(
            animatorHandleId);
    if (physicalHandleId < 0 || commands.empty())
      // ignore
      return;

    auto nmbrOfCommands = commands.size();
    UartKeysMessage msg(physicalHandleId, (u8)nmbrOfCommands);
    for (std::size_t idx = 0; idx < nmbrOfCommands; ++idx) {
      msg.set_key(idx, commands[idx].asInflatedCmdKey().raw);
    }

    if (true) {
      ESP_LOGI(TAG, "send(S%02d, A%d->PA%d size: %d", animatorHandleId >> 1,
               animatorHandleId, physicalHandleId, commands.size());

      if (false) {
        for (std::size_t idx = 0; idx < nmbrOfCommands; ++idx) {
          const auto cmd = InflatedCmdKey(msg.get_key(idx));
          if (cmd.extended()) {
            const auto extended_cmd = cmd.steps();
            ESP_LOGI(TAG, "Cmd: extended_cmd=%d", int(extended_cmd));
          } else {
            const auto ghosting = cmd.ghost();
            const auto steps = cmd.steps();
            const auto speed = cmdSpeedUtil.deflate_speed(cmd.inflated_speed());
            const auto clockwise = cmd.clockwise();
            const auto relativePosition = true;
            ESP_LOGI(TAG, "Cmd: %s=%3d sp=%d gh=%s cl=%s",
                     ghosting || relativePosition ? "steps" : "   to", steps,
                     speed, YESNO(ghosting),
                     ghosting ? "N/A" : YESNO(clockwise));
          }
        }
        ESP_LOGI(TAG, "  done");
      }
    }
    channel->send(msg);
  }

  void sendAndClear(int handleId, std::vector<DeflatedCmdKey> &selected) {
    if (selected.size() > 0) {
      sendCommandsForHandle(handleId, selected);
      selected.clear();
    }
  }

  void sendCommands(std::vector<HandleCmd> &cmds) {
    std::sort(std::begin(cmds), std::end(cmds),
              [](const HandleCmd &a, const HandleCmd &b) {
                return a.handleId == b.handleId ? a.orderId < b.orderId
                                                : a.handleId < b.handleId;
              });
    std::vector<DeflatedCmdKey> selected;
    int lastHandleId = -1;
    int lastHandleMessages = 0;
    for (auto it = std::begin(cmds); it != std::end(cmds); ++it) {
      const auto &handleCmd = *it;
      if (handleCmd.ignorable()) {
        // ignore ('deleted')
        continue;
      }
      if (handleCmd.handleId != lastHandleId) {
        sendAndClear(lastHandleId, selected);
        ESP_LOGW(TAG, "H%d : %d messages", lastHandleId, lastHandleMessages);

        lastHandleId = handleCmd.handleId;
        lastHandleMessages = 0;
      }
      lastHandleMessages++;
      selected.push_back(handleCmd.cmd);
      if (selected.size() == MAX_ANIMATION_KEYS_PER_MESSAGE) {
        sendAndClear(lastHandleId, selected);
      }
    }
    sendAndClear(lastHandleId, selected);
    ESP_LOGW(TAG, "H%d : %d messages", lastHandleId, lastHandleMessages);
    cmds.clear();
  }

  void sendInstructions(Instructions &instructions, u32 millisLeft = u32(-1)) {
    updateSpeeds(instructions);

    // lets start transmitting
    channel->send(Message(-1, MsgEnum::MSG_BEGIN_KEYS));

    // send instructions
    sendCommands(instructions.cmds);
    instructions.dump();

    // finalize
    channel->send(
        UartEndKeysMessage(instructions.turn_speed, instructions.turn_steps,
                           cmdSpeedUtil.get_speeds(),
                           instructions.get_speed_detection(), millisLeft));
    ESP_LOGI(TAG, "millisLeft=%ld", long(millisLeft));
  }
};

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

DistanceCalculators::Func selectDistanceCalculator(
    const HandlesDistanceEnum &value) {
  switch (value) {
#define CASE(WHAT, FUNC)          \
  case HandlesDistanceEnum::WHAT: \
    return DistanceCalculators::FUNC;

    CASE(Shortest, shortest)
    CASE(Right, clockwise)
    CASE(Left, antiClockwise)
    default:
      CASE(Random, random())
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

void send_text(BufferChannel *channel, AnimationController *controller,
               const animation::AnimationSettings &settings, const Text &text) {
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

  Transmitter transmitter(controller, channel);
  transmitter.sendInstructions(instructions, millis_left);
}

#endif