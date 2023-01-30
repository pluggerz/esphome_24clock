#pragma once

#include <vector>

#include "../bmatrix_mdi/bmatrix_mdi.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/core/component.h"

namespace esphome {
namespace bmatrix {

using esphome::Color;
using esphome::display::Font;
using esphome::display::TextAlign;
using esphome::text_sensor::TextSensor;

class Widget : public Canvas {
 public:
};

class Group : public esphome::Component, public Widget {
  const char* const TAG = "Group";

 public:
  std::vector<Widget*> widgets;

  void add(Widget* widget) { widgets.push_back(widget); }

  virtual ScratchItem render(Scratch& parent_scratch) override {
    auto ret = parent_scratch.create(this);

    auto previous_alert_owner = parent_scratch.alert_owner;
    parent_scratch.alert_owner = this;

    int x = 0;
    for (auto widget : widgets) {
      auto item = widget->render(parent_scratch);
      item.x = x;
      x += item.width;
      ret.height = std::max(item.height, ret.height);
      ret.stack.push_back(item);
    }
    ret.width = x;
    parent_scratch.alert_owner = previous_alert_owner;
    return ret;
  }

  virtual void draw(DisplayBuffer* display_buffer,
                    const ScratchItem& item) override {
    for (auto draw_widget : item.stack) {
      if (draw_widget.owner) {
        draw_widget.owner->draw(display_buffer, draw_widget);
      }
    }
  }
};
}  // namespace bmatrix
}  // namespace esphome