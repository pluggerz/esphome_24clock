#pragma once

#ifdef USE_TEXT_SENSOR
#include "bmatrix_common.h"

namespace esphome {
namespace bmatrix {

class TextSensorWidget : public TextBasedWidget {
 protected:
  const TextSensor* text_sensor;

  virtual ScratchItem render(Scratch& parent_scratch) override {
    return render_text(parent_scratch, this->text_sensor->get_state());
  }

 public:
  TextSensorWidget(const TextSensor* text_sensor) : text_sensor(text_sensor) {}
};

}  // namespace bmatrix
}  // namespace esphome
#endif