#include "stub.h"

#if defined(IS_DIRECTOR)

#include "esphome/core/log.h"

using namespace esphome;

#endif

#include "../clocks_director/keys.h"

using keys::CmdSpeedUtil;
using keys::InflatedCmdKey;

const char *const TAG = "keyss";

// TODO: static or object not like this !?
keys::CmdSpeedUtil keys::cmdSpeedUtil;

#if defined(IS_DIRECTOR)
void keys::DeflatedCmdKey::dump(const char *extra) const {
  ESP_LOGE(TAG, "%s: gh=%s cl=%s, ab=%s, steps=%d, sp=%d raw=%d", extra,
           YESNO(ghost()), YESNO(clockwise()), YESNO(absolute()), steps(),
           speed(), (int)raw);
}

void InflatedCmdKey::dump(const char *extra) const {
  if (extended()) {
    ESP_LOGE(TAG, "%s: ex=%s", extra, YESNO(extended()));
  } else {
    ESP_LOGE(TAG, "%s: gh=%s cl=%s, ex=%s, steps=%d", extra, YESNO(ghost()),
             YESNO(clockwise()), YESNO(extended()), steps());
  }
}

#endif
const CmdSpeedUtil::Speeds &CmdSpeedUtil::get_speeds() const { return speeds; }

void CmdSpeedUtil::set_speeds(const CmdSpeedUtil::Speeds &speeds) {
  for (int idx = 0; idx <= max_inflated_speed; ++idx) {
    auto value = speeds[idx];
    this->speeds[idx] = value;
  }
#if defined(IS_DIRECTOR)
  ESP_LOGD(TAG, "Speeds(%d,%d,%d,%d,%d,%d,%d,%d)", speeds[0], speeds[1],
           speeds[2], speeds[3], speeds[4], speeds[5], speeds[6], speeds[7]);
#endif
}

#if defined(IS_DIRECTOR)
uint8_t CmdSpeedUtil::inflate_speed(const uint8_t value) const {
  uint8_t inflated_speed = max_inflated_speed;
  while (inflated_speed > 0) {
    if (value >= speeds[inflated_speed]) return inflated_speed;
    inflated_speed--;
  }
  return 0;
}
#endif
