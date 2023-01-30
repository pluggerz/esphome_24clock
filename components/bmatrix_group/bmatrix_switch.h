#pragma once
#ifdef USE_SWITCH
#include "bmatrix_common.h"
#include "esphome/components/switch/switch.h"

namespace esphome {
namespace bmatrix {
using esphome::switch_::Switch;
typedef Switch* SwitchPntr;

class SwitchWidget : public OnOffStateWidget {
  const SwitchPntr switch_;

  virtual bool get_render_state() const override { return switch_->state; }

 public:
  SwitchWidget(SwitchPntr switch_) : switch_(switch_) {}
};
}  // namespace bmatrix
}  // namespace esphome
#endif