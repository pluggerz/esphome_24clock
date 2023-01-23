#include "transmitter.h"

#include <set>

#include "../clocks_shared/async.interop.h"
#include "../clocks_shared/channel.interop.h"

const char *const TAG = "transmitter";

using namespace transmitter;

using async::async_interop;
using channel::Message;
using channel::MsgEnum;
using channel::messages::UartEndKeysMessage;
using channel::messages::UartKeysMessage;
using keys::cmdSpeedUtil;
using keys::CmdSpeedUtil;
using keys::InflatedCmdKey;

void Transmitter::sendCommandsForHandle(
    int animatorHandleId, const std::vector<keys::DeflatedCmdKey> &commands) {
  const auto physicalHandleId =
      animation_controller->mapAnimatorHandle2PhysicalHandleId(
          animatorHandleId);
  if (physicalHandleId < 0 || commands.empty())
    // ignore
    return;

  const auto nmbrOfCommands = commands.size();
  UartKeysMessage msg(physicalHandleId, (u8)nmbrOfCommands);
  for (std::size_t idx = 0; idx < nmbrOfCommands; ++idx) {
    msg.set_key(idx, commands[idx].asInflatedCmdKey().raw);
  }
  async_interop.direct_message(msg);

  LOGI(TAG, "send(S%02d, A%d->PA%d size: %d", animatorHandleId >> 1,
       animatorHandleId, physicalHandleId, commands.size());

  // NOTE: executing (all) the code below gives the performer time as well
  delay(2);
  for (std::size_t idx = 0; idx < nmbrOfCommands; ++idx) {
    const auto cmd = InflatedCmdKey(msg.get_key(idx));
    if (cmd.extended()) {
      const auto extended_cmd = cmd.steps();
      LOGI(TAG, "Cmd: extended_cmd=%d", int(extended_cmd));
    } else {
      const auto ghosting = cmd.ghost();
      const auto steps = cmd.steps();
      const auto speed = cmdSpeedUtil.deflate_speed(cmd.inflated_speed());
      const auto clockwise = cmd.clockwise();
      const auto relativePosition = true;
      LOGI(TAG, "Cmd: %s=%3d sp=%d gh=%s cl=%s",
           ghosting || relativePosition ? "steps" : "   to", steps, speed,
           YESNO(ghosting), ghosting ? "N/A" : YESNO(clockwise));
    }
    LOGI(TAG, "  done");
  }
}

void Transmitter::sendCommands(std::vector<HandleCmd> &cmds) {
  std::sort(std::begin(cmds), std::end(cmds),
            [](const HandleCmd &a, const HandleCmd &b) {
              return a.handleId == b.handleId ? a.orderId < b.orderId
                                              : a.handleId < b.handleId;
            });
  std::vector<DeflatedCmdKey> selected;
  int lastHandleId = -1;
  int lastHandleMessages = 0;
  // bool seenHandleIds[MAX_HANDLES];
  // for (int handleId = 0; handleId < MAX_HANDLES; handleId++)
  //  seenHandleIds[handleId] = false;

  for (auto it = std::begin(cmds); it != std::end(cmds); ++it) {
    const auto &handleCmd = *it;
    if (handleCmd.ignorable()) {
      // ignore ('deleted')
      continue;
    }
    // seenHandleIds[handleCmd.handleId] = true;
    if (handleCmd.handleId != lastHandleId) {
      if (lastHandleId != -1) {
        sendAndClear(lastHandleId, selected);
        ESP_LOGW(TAG, "H%d : %d messages", lastHandleId, lastHandleMessages);
      }

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
  // for (int handleId = 0; handleId < MAX_HANDLES; handleId++) {
  //   if (seenHandleIds[handleId]) continue;
  //  ESP_LOGW(TAG, "H%d : dummy messages", lastHandleId);
  // }

  cmds.clear();
}

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

void Transmitter::sendInstructions(Instructions &instructions, u32 millisLeft) {
  updateSpeeds(instructions);

  // lets start transmitting
  async_interop.direct_message(Message(-1, MsgEnum::MSG_BEGIN_KEYS));

  // send instructions
  sendCommands(instructions.cmds);
  instructions.dump();

  // finalize
  async_interop.direct_message(
      UartEndKeysMessage(instructions.turn_speed, instructions.turn_steps,
                         cmdSpeedUtil.get_speeds(),
                         instructions.get_speed_detection(), millisLeft));
  LOGI(TAG, "millisLeft=%ld", long(millisLeft));
}