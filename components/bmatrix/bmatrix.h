#pragma once

#include <map>

#include "esphome/components/display/display_buffer.h"
#include "esphome/core/color.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome {
namespace bmatrix {

using esphome::Color;
using esphome::display::DisplayBuffer;
using esphome::display::Font;
using esphome::display::TextAlign;

typedef int Millis;

class Owner {};
class Canvas;
class BMatrix;
class Mdi;
typedef Mdi* MdiPntr;

class ScratchItem {
 public:
  Canvas* owner;
  Font* font = nullptr;
  Mdi* mdi = nullptr;
  std::string text;
  std::vector<ScratchItem> stack;
  TextAlign align;
  Color color;
  int x = 0;
  int y = 0;
  int width = 0;
  int height = 0;

  ScratchItem(Canvas* owner) : owner(owner) {}
};

struct DelayInfo {
  const char* const TAG = "DelayInfo";

 public:
  struct Delay {
    int index = 0;
    int t0 = 0;
  };
  std::map<int, Delay> delays;
  void update_delays() {
    int now = esphome::millis();
    for (auto& item : delays) {
      auto& value = item.second;
      if (now - value.t0 > item.first) {
        value.index++;
        value.t0 = now;
        ESP_LOGD(TAG, "Increment delay=%d to index=%d", item.first,
                 value.index);
      }
    }
  }

  int get_delay_index(int delay) {
    auto result = delays.find(delay);
    if (result != delays.end()) {
      return result->second.index;
    }
    ESP_LOGI(TAG, "Added delay=%d", delay);
    delays.insert(std::pair<int, Delay>(delay, Delay()));
    return 0;
  }
};

struct Scratch {
  const char* const TAG = "Scratch";

  std::vector<ScratchItem> stack;

  BMatrix* bmatrix = nullptr;
  int x = 0, y = 0;
  Font* font = nullptr;
  Mdi* mdi = nullptr;
  TextAlign align = TextAlign::TOP_LEFT;
  Color color = esphome::display::COLOR_ON;
  DisplayBuffer* display_buffer;
  DelayInfo* delay_info = nullptr;
  Canvas* alert_owner = nullptr;
  MdiPntr get_mdi() const { return this->mdi; }

  std::optional<Millis> age_alert_in_millis();

  int get_delay_index(int delay) {
    if (delay_info == nullptr) {
      ESP_LOGW(TAG, "field delay_info not initialized properly!?");
      return 0;
    }
    return delay_info->get_delay_index(delay);
  }

  void from_parent(const Scratch& parent_scratch, Font* font = nullptr) {
    this->font = font == nullptr ? parent_scratch.font : font;
    this->display_buffer = parent_scratch.display_buffer;
    this->color = parent_scratch.color;
    this->mdi = parent_scratch.mdi;
    this->delay_info = parent_scratch.delay_info;
    this->alert_owner = parent_scratch.alert_owner;
    this->bmatrix = parent_scratch.bmatrix;
  }

  void push_on_stack(const ScratchItem& item) { stack.push_back(item); }
  void push_on_stack(const Scratch& scratch) {
    stack.insert(stack.end(), scratch.stack.begin(), scratch.stack.end());
  }

  ScratchItem create(Canvas* owner);
};

class Canvas {
 protected:
  Font* font = nullptr;

 public:
  void set_font(Font* value) { this->font = value; }
  Font* get_font() const { return this->font; }

  virtual ~Canvas() {}
  virtual ScratchItem render(Scratch& scratch) { return ScratchItem(nullptr); }
  virtual void draw(DisplayBuffer* display_buffer, const ScratchItem& item) {}
};

class BMatrix : public esphome::Component, public Canvas, Owner {
  const char* const TAG = "bmatrix";

 public:
  Mdi* mdi = nullptr;
  Canvas* default_group = nullptr;
  struct AlertGroup {
    Canvas* canvas;
    Millis t0;
    AlertGroup(Canvas* canvas) : canvas(canvas), t0(millis()) {}
  };
  std::vector<AlertGroup> alert_groups;

  DisplayBuffer* display_buffer;
  DelayInfo delay_info;

 public:
  BMatrix(DisplayBuffer* display_buffer) : display_buffer(display_buffer) {}
  void set_canvas(Canvas* value) { this->default_group = value; }
  void set_mdi(Mdi* value) { this->mdi = value; }

  std::optional<Millis> age_alert_in_millis(Canvas* canvas) {
    for (auto& it : alert_groups) {
      if (it.canvas == canvas) {
        return {esphome::millis() - it.t0};
      }
    }
    return {};
  }

  void add_alert(Canvas* canvas) {
    for (auto& it : alert_groups) {
      if (it.canvas == canvas) {
        it.t0 = millis();
        return;
      }
    }
    this->alert_groups.push_back(canvas);
  }
  bool remove_alert(Canvas* canvas) {
    auto it = alert_groups.begin();
    while (it != alert_groups.end()) {
      if (it->canvas == canvas) {
        alert_groups.erase(it);
        return true;
      }
      ++it;
    }
    return false;
  }

  void draw() {
    delay_info.update_delays();
    for (auto& it : alert_groups) {
      // TBA
    }

    Scratch scratch;
    scratch.bmatrix = this;
    scratch.font = get_font();
    scratch.display_buffer = display_buffer;
    scratch.mdi = mdi;
    scratch.delay_info = &delay_info;
    if (scratch.font == nullptr) {
      ESP_LOGE(TAG, "No font selected !?");
      return;
    }

    Canvas* canvas;
    if (alert_groups.size() > 0) {
      canvas = alert_groups[0].canvas;
    } else {
      canvas = default_group;
    }
    if (!canvas) {
      ESP_LOGE(TAG, "No canvas selected !?");
      return;
    }
    auto item = canvas->render(scratch);
    if (item.owner) {
      item.owner->draw(display_buffer, item);
    }
  }
};

}  // namespace bmatrix
}  // namespace esphome