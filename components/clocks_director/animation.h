#pragma once

// #include "oclock.h"
#include "../clocks_director/keys.h"
#include "../clocks_shared/ticks.h"

class AnimationController {
  int16_t tickz[MAX_HANDLES] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
                                -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
                                -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
                                -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};

  int clockId2animatorId[MAX_SLAVES] = {-1, -1, -1, -1, -1, -1, -1, -1,
                                        -1, -1, -1, -1, -1, -1, -1, -1,
                                        -1, -1, -1, -1, -1, -1, -1, -1};
  int animatorId2clockId[MAX_SLAVES] = {-1, -1, -1, -1, -1, -1, -1, -1,
                                        -1, -1, -1, -1, -1, -1, -1, -1,
                                        -1, -1, -1, -1, -1, -1, -1, -1};

 public:
  void reset_handles() {
    for (int idx = 0; idx < MAX_HANDLES; ++idx) tickz[idx] = -1;
  }
  void set_ticks_for_performer(int performer_id, int ticks0, int ticks1);

  int getCurrentTicksForAnimatorHandleId(int animatorHandleId) {
    return animatorHandleId < 0 || animatorHandleId >= MAX_HANDLES
               ? -1
               : tickz[animatorHandleId];
  }

  void dump_config();

  void remap(int animator_id, int clockId);

  int mapAnimatorHandle2PhysicalHandleId(int animator_id) {
    int add_one = animator_id & 1;
    return add_one + (animatorId2clockId[animator_id >> 1] << 1);
  }
};

class HandleCmd {
 public:
  inline bool ignorable() const { return handleId == -1; }
  int handleId;
  keys::DeflatedCmdKey cmd;
  int orderId;
  inline uint8_t speed() const { return cmd.speed(); }
  inline bool ghost_or_alike() const { return cmd.ghost() || cmd.absolute(); }
  HandleCmd() : handleId(-1), cmd(keys::DeflatedCmdKey()), orderId(-1) {}
  HandleCmd(int handleId, keys::DeflatedCmdKey cmd, int orderId)
      : handleId(handleId), cmd(cmd), orderId(orderId) {}
};

class Flags {
 private:
  uint64_t flags = 0xFFFFFFFFFFFFFFFF;

 public:
  const uint64_t &raw() const { return flags; }

  void copyFrom(const Flags &src) { flags = src.flags; }

  bool operator[](int handleId) const {
    return ((flags >> handleId) & (uint64_t)1) == 1;
  }
  void hide(int handleId) { flags = flags & ~((uint64_t)1 << handleId); }
};

class HandlesState {
 protected:
  int16_t tickz[MAX_HANDLES] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
                                -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
                                -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
                                -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
  double timers[MAX_HANDLES] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

 public:
  Flags visibilityFlags, nonOverlappingFlags;

  double time_at(int handle_id) const { return timers[handle_id]; }

  inline bool valid_handle(int handleId) const { return tickz[handleId] >= 0l; }
  inline bool valid_handles(int handle_id_0, int handle_id_1) const {
    return valid_handle(handle_id_0) && valid_handle(handle_id_1);
  }

  void setAll(int longHandle, int shortHandle) {
    for (int idx = 0; idx < MAX_HANDLES; ++idx)
      tickz[idx] = 1 == (idx & 1) ? longHandle : shortHandle;
  }

  void setIds() {
    int m = NUMBER_OF_STEPS / 12;
    for (int idx = 0; idx < MAX_SLAVES; ++idx) {
      int handle1 = idx / 2;
      int handle2 = idx - handle1;
      tickz[idx * 2] = handle1 * m;
      tickz[idx * 2 + 1] = handle2 * m;
    }
  }

  void dump();

  void copyFrom(const HandlesState &src) {
    for (int idx = 0; idx < MAX_HANDLES; ++idx) tickz[idx] = src[idx];
    visibilityFlags.copyFrom(src.visibilityFlags);
    nonOverlappingFlags.copyFrom(src.nonOverlappingFlags);
  }

  void set_ticks(int handle_id, int ticks) {
    tickz[handle_id] = Ticks::normalize(ticks);
  }
  /*
  int16_t &operator[](int handleId)
  {
      return tickz[handleId];
  }*/
  const int16_t &operator[](int handleId) const { return tickz[handleId]; }
};

#include <map>

class HandleBitMask {
  uint64_t bits{0};
};

class Instructions : public HandlesState {
 public:
  static const bool send_relative;
  std::vector<HandleCmd> cmds;
  uint64_t speed_detection{~uint64_t(0)};
  static int turn_speed;
  static int turn_steps;
  AnimationController *animationController;

  void iterate_handle_ids(std::function<void(int handle_id)> func) {
    for (int handle_id = 0; handle_id < MAX_HANDLES; ++handle_id) {
      if (!valid_handle(handle_id)) {
        continue;
      }
      func(handle_id);
    };
  }

  const uint64_t &get_speed_detection() const { return speed_detection; }

  void set_detect_speed_change(int animation_handle_id, bool value);

  Instructions(AnimationController *animationController)
      : animationController(animationController) {
    for (auto handleId = 0; handleId < MAX_HANDLES; handleId++)
      tickz[handleId] =
          this->animationController->getCurrentTicksForAnimatorHandleId(
              handleId);
  }

  void rejectInstructions(int firstHandleId, int secondHandleId) {
    std::for_each(
        cmds.begin(), cmds.end(),
        [firstHandleId, secondHandleId](HandleCmd &cmd) {
          if (cmd.handleId == firstHandleId || cmd.handleId == secondHandleId)
            cmd.handleId = -1;
        });
  }

  void rejectInstructions(int handleId) {
    rejectInstructions(handleId, handleId);
  }

  void addAll(const keys::DeflatedCmdKey &cmd) {
    for (int handleId = 0; handleId < MAX_HANDLES; ++handleId) {
      add(handleId, cmd);
    }
  }

  void add(int handle_id, const keys::DeflatedCmdKey &cmd) {
    if (cmd.steps() == 0 && cmd.relative()) {
      // ignore, since we are talking about steps
      return;
    }
    add_(handle_id, as_relative_cmd(handle_id, cmd));
  }

  inline keys::DeflatedCmdKey as_relative_cmd(const int handle_id,
                                              const keys::DeflatedCmdKey &cmd) {
    const auto relativePosition = cmd.relative();
    if (relativePosition) return cmd;

    // make relative
    const auto from_tick = tickz[handle_id];
    const auto to_tick = cmd.steps();
    return keys::DeflatedCmdKey(
        cmd.mode() - keys::CmdEnum::ABSOLUTE,
        cmd.clockwise() ? Distance::clockwise(from_tick, to_tick)
                        : Distance::antiClockwise(from_tick, to_tick),
        cmd.speed());
  }

  void add(int handle_id, const keys::CmdSpecialMode &mode) {
    auto cmd = keys::DeflatedCmdKey(keys::CmdEnum::ABSOLUTE, mode, 0);
    cmds.push_back(HandleCmd(handle_id, cmd, cmds.size()));
  }

  void follow_seconds(int handle_id, bool discrete) {
    add(handle_id, discrete ? keys::CmdSpecialMode::FOLLOW_SECONDS_DISCRETE
                            : keys::CmdSpecialMode::FOLLOW_SECONDS);
  }

  /***
   * NOTE: cmd is relative
   */
  void add_(int handle_id, const keys::DeflatedCmdKey &cmd);
};
