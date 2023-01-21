#include "clocks_light.h"

#include "../clocks_shared/async.interop.h"
#include "../clocks_shared/channel.interop.h"
#include "../clocks_shared/log.h"

using namespace esphome::clocks_light;
using channel::Message;
using channel::MsgEnum;

void ClocksLightOutput::transmit() {
  int leds_idx = 0;
  for (int performer_id = 0; performer_id < 24; ++performer_id) {
    channel::messages::IndividualLeds msg(performer_id);
    for (int idx = 0; idx < 12; ++idx) {
      auto &out = msg.leds[idx];
      const auto &in = this->dirty_leds[leds_idx++];
      out.r = in.r >> 3;
      out.g = in.g >> 2;
      out.b = in.b >> 3;
    }

    async::async_interop.direct_message(msg);
  }
  async::async_interop.direct_message(
      Message(-1, MsgEnum::MSG_INDIVIDUAL_LEDS_SHOW));
  dirty = false;
  // LOGI(TAG, "ClocksLightOutput::transmit!");
}

void ClocksLightOutput::loop() {
  if (async::interop::suspended) return;
  // LOGI(TAG, "ClocksLightOutput::loop!");

  // if (true) return;
  AddressableLight::loop();

  if (!this->dirty) {
    return;
  }

  if (millis() - this->last_send_in_millis > 80) {
    transmit();
    this->last_send_in_millis = millis();
  }
}

void ClocksLightOutput::dump() {
  for (int i = 0; i < this->size(); i++)
    this->dirty_leds[i] = this->internal_view_leds[i];

  this->dirty = true;
  // loop();
}