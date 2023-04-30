#pragma once

#include "../clocks_shared/log.h"
#include "../clocks_shared/shared.types.h"
#include "esphome/components/light/addressable_light.h"
#include "esphome/components/light/light_output.h"
#include "esphome/core/color.h"
#include "esphome/core/component.h"

namespace esphome {
namespace clocks_light {
using light::AddressableLight;

constexpr int TOTAL_LEDS = 12 * 6 * 4;

const char *const TAG = "clocks_light";

class ClocksLightOutput : public AddressableLight {
 private:
  void transmit();

  int32_t size() const override { return TOTAL_LEDS; }

  void clear_effect_data() override {}

  light::LightTraits get_traits() override {
    auto traits = light::LightTraits();
    traits.set_supported_color_modes({light::ColorMode::RGB});
    return traits;
  }

  light::ESPColorView get_view_internal(int32_t index) const override {
    Color *base = (Color *)&internal_view_leds[index];
    return light::ESPColorView(&base->red, &base->green, &base->blue, nullptr,
                               nullptr, &this->correction_);
  }

  // ========== INTERNAL METHODS ==========
  void setup() override {
    for (int i = 0; i < this->size(); i++) {
      (*this)[i] = Color(0, 0, 0, 0);
    }
  }

  void dump();

  void write_state(light::LightState *state) override { dump(); }

  virtual void loop() override;

 public:
  void add_leds(int nmbr) {
    // ignored for now
  }

 protected:
  uint8_t rgb_offsets_[4]{0, 1, 2, 3};

 public:
  Color internal_view_leds[TOTAL_LEDS];
  Color snapshot[TOTAL_LEDS];

  bool dirty = false;
  Millis last_send_in_millis = 0L;
};
}  // namespace clocks_light
}  // namespace esphome