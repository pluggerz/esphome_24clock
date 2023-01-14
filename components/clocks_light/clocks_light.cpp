#include "clocks_light.h"

#include "../clocks_shared/async.interop.h"
#include "../clocks_shared/channel.interop.h"
#include "../clocks_shared/log.h"

using namespace esphome::clocks_light;

using async::Async;
using async::DelayAsync;
using async::OneshotAsync;

class DelayedTransmitAsync : public DelayAsync {
  ClocksLightOutput *owner;

 public:
  DelayedTransmitAsync(ClocksLightOutput *owner)
      : DelayAsync(20), owner(owner) {}

  void execute() {
    int leds_idx = 0;
    for (int idx = 0; idx < 2; ++idx) {
      channel::messages::IndividualLeds msg(idx);
      for (int idx = 0; idx < 12; ++idx) {
        auto &out = msg.leds[idx];
        const auto &in = owner->leds[leds_idx++];
        out.r = in.r;
        out.g = in.g;
        out.b = in.b;
      }
      async::async_interop.direct_message(msg);
    }
    owner->in_queue = false;
  }

  virtual Async *update() override {
    Millis now = millis();
    if (now - owner->last_send_in_millis > 20) {
      execute();
      owner->last_send_in_millis = now;
      return nullptr;
    }
    return this;
  }
};

void ClocksLightOutput::dump() {
  if (this->in_queue) {
    return;
  }
  this->in_queue = true;
  async::async_interop.queue_async(new DelayedTransmitAsync(this));
}