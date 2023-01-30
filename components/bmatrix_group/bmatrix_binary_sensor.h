#pragma once
#ifdef USE_BINARY_SENSOR
#include "bmatrix_common.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

namespace esphome {
namespace bmatrix {
using esphome::binary_sensor::BinarySensor;
typedef BinarySensor* BinarySensorPntr;

class BinarySensorWidget : public OnOffStateWidget {
  const BinarySensorPntr sensor;

  virtual bool get_render_state() const override { return sensor->state; }

 public:
  BinarySensorWidget(BinarySensorPntr sensor) : sensor(sensor) {
    set_device_class(sensor->get_device_class());
  }
};
}  // namespace bmatrix
}  // namespace esphome
#endif