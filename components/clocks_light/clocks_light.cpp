#include "clocks_light.h"

#include "../clocks_shared/async.interop.h"
#include "../clocks_shared/channel.interop.h"
#include "../clocks_shared/log.h"

using namespace esphome::clocks_light;
using channel::Message;
using channel::MsgEnum;

void ClocksLightOutput::transmit() {
  int leds_idx = 0;
  for (int idx = 0; idx < 2; ++idx) {
    channel::messages::IndividualLeds msg(idx);
    for (int idx = 0; idx < 12; ++idx) {
      auto &out = msg.leds[idx];
      const auto &in = this->leds[leds_idx++];
      out.r = in.r;
      out.g = in.g;
      out.b = in.b;
    }

    async::async_interop.direct_message(msg);
  }
  async::async_interop.direct_message(
      Message(-1, MsgEnum::MSG_INDIVIDUAL_LEDS_SHOW));
}

void ClocksLightOutput::loop() {
  AddressableLight::loop();

  if (!this->dirty) {
    return;
  }

  Millis now = millis();
  if (now - this->last_send_in_millis > 20) {
    transmit();
    this->last_send_in_millis = now;
    this->dirty = false;
  }
}

void ClocksLightOutput::dump() { this->dirty = true; }