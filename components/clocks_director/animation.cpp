#include "../clocks_shared/stub.h"

#if defined(IS_DIRECTOR)
#include "animation.h"
#include "esphome/core/log.h"

using namespace esphome;

const char *const TAG = "animation";

void HandlesState::dump() {
  ESP_LOGI(TAG, "HandlesState:");
#define DRAW_ROW(S)                                                            \
  ESP_LOGI(                                                                    \
      TAG,                                                                     \
      "   A%02d(%3d %3d)    A%02d(%3d %3d)   A%02d(%3d %3d)   A%02d(%3d %3d) " \
      "  A%02d(%3d %3d)   A%02d(%3d %3d)   A%02d(%3d %3d)   A%02d(%3d %3d)",   \
      S + 0, tickz[(S + 0) * 2 + 0], tickz[(S + 0) * 2 + 1], S + 1,            \
      tickz[(S + 1) * 2 + 0], tickz[(S + 1) * 2 + 1], S + 6,                   \
      tickz[(S + 6) * 2 + 0], tickz[(S + 6) * 2 + 1], S + 7,                   \
      tickz[(S + 7) * 2 + 0], tickz[(S + 7) * 2 + 1], S + 12,                  \
      tickz[(S + 12) * 2 + 0], tickz[(S + 12) * 2 + 1], S + 13,                \
      tickz[(S + 13) * 2 + 0], tickz[(S + 13) * 2 + 1], S + 18,                \
      tickz[(S + 18) * 2 + 0], tickz[(S + 18) * 2 + 1], S + 19,                \
      tickz[(S + 19) * 2 + 0], tickz[(S + 19) * 2 + 1]);
#define DRAW_ROW2(S)                                                           \
  ESP_LOGI(                                                                    \
      TAG,                                                                     \
      "      %3.2f %3.2f       %3.2f %3.2f      %3.2f %3.2f      %3.2f %3.2f " \
      "     %3.2f %3.2f      %3.2f %3.2f      %3.2f %3.2f      %3.2f %3.2f",   \
      timers[(S + 0) * 2 + 0], timers[(S + 0) * 2 + 1],                        \
      timers[(S + 1) * 2 + 0], timers[(S + 1) * 2 + 1],                        \
      timers[(S + 6) * 2 + 0], timers[(S + 6) * 2 + 1],                        \
      timers[(S + 7) * 2 + 0], timers[(S + 7) * 2 + 1],                        \
      timers[(S + 12) * 2 + 0], timers[(S + 12) * 2 + 1],                      \
      timers[(S + 13) * 2 + 0], timers[(S + 13) * 2 + 1],                      \
      timers[(S + 18) * 2 + 0], timers[(S + 18) * 2 + 1],                      \
      timers[(S + 19) * 2 + 0], timers[(S + 19) * 2 + 1]);

  DRAW_ROW(0);
  DRAW_ROW2(0);
  DRAW_ROW(2);
  DRAW_ROW2(2);
  DRAW_ROW(4);
  DRAW_ROW2(4);
#undef DRAW_ROW
  // INFO("visibilityFlags: " <<
  // std::bitset<MAX_HANDLES>(visibilityFlags.raw()));
  // INFO("nonOverlappingFlags: " <<
  // std::bitset<MAX_HANDLES>(nonOverlappingFlags.raw()));
}

void AnimationController::set_ticks_for_performer(int performer_id, int ticks0,
                                                  int ticks1) {
  auto animator_id = clockId2animatorId[performer_id];
  if (animator_id == -1) {
    ESP_LOGE(TAG, "P%d not mapped!?", performer_id);
    return;
  }
  auto handle_id = animator_id << 1;
  tickz[handle_id + 0] = ticks0;
  tickz[handle_id + 1] = ticks1;
}

void AnimationController::dump_config() {
  ESP_LOGI(TAG, "  animation_controller:");
  for (int idx = 0; idx < NMBR_OF_PERFORMERS; idx++) {
    auto animator_id = clockId2animatorId[idx];
    if (animator_id == -1) continue;
    auto handleId = animator_id << 1;
    ESP_LOGI(TAG, "   P%d -> A%d: ticks(%d, %d)", idx, clockId2animatorId[idx],
             tickz[handleId], tickz[handleId + 1]);
  }
}

void AnimationController::remap(int animator_id, int clockId) {
  animatorId2clockId[animator_id] = clockId;
  clockId2animatorId[clockId] = animator_id;
  ESP_LOGI(TAG, "added mapping: S%d <-> A%d", clockId, animator_id);
}

void Instructions::set_detect_speed_change(int animation_handle_id,
                                           bool value) {
  auto actual_handle_id =
      animationController->mapAnimatorHandle2PhysicalHandleId(
          animation_handle_id);
  if (actual_handle_id < 0) {
    // ignore
    ESP_LOGE(TAG, "Not mapped: animation_handle_id=%d", animation_handle_id);
    return;
  }

  // note: uint64_t is essential
  uint64_t bit = uint64_t(1) << uint64_t(actual_handle_id);
  if (value)
    speed_detection |= bit;
  else
    speed_detection &= ~bit;
  ESP_LOGE(TAG,
           "Mapped: animation_handle_id=%d -> actual_handle_id=%d and bit=%llu",
           animation_handle_id, actual_handle_id, speed_detection);
}

void Instructions::add_(int handle_id, const keys::DeflatedCmdKey &cmd) {
  // calculate time
  timers[handle_id] += cmd.time();
  cmds.push_back(HandleCmd(handle_id, cmd, cmds.size()));

  const auto ghosting = cmd.ghost();
  if (ghosting)
    // stable ;P, just make sure time is aligned
    return;

  auto &current = tickz[handle_id];
  if (current < 0) {
    ESP_LOGE(TAG,
             "Mmmm I think you did something wrong... hanlde_id=%d has no "
             "known state!?",
             handle_id);
  }
  current = Ticks::normalize(current +
                             (cmd.clockwise() ? cmd.steps() : -cmd.steps()));
};
#endif