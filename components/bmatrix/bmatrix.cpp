#include "bmatrix.h"

using namespace esphome::bmatrix;

ScratchItem Scratch::create(Canvas* owner) {
  ScratchItem ret(owner);
  ret.font = owner->get_font();
  if (!ret.font) ret.font = font;
  ret.x = this->x;
  ret.y = this->y;
  ret.color = this->color;
  ret.align = this->align;
  ret.mdi = this->mdi;
  return ret;
}

std::optional<Millis> Scratch::age_alert_in_millis() {
  return bmatrix->age_alert_in_millis(alert_owner);
}