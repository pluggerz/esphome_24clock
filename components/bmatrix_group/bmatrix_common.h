#pragma once

#include <memory>

#include "bmatrix_group.h"

namespace esphome {
namespace bmatrix {

class TextBasedWidget : public Widget {
  const char* const TAG = "TextBasedWidget";

 protected:
  static void print(ScratchItem& item, DisplayBuffer* display_buffer,
                    Font* font, Color color, TextAlign align,
                    const char* text) {
    int x_start = 0, y_start = 0;
    int width = 0, height = 0;
    display_buffer->get_text_bounds(0, 0, text, font, align, &x_start, &y_start,
                                    &width, &height);
    item.width = x_start + width;
    item.height = y_start + height;
  }

  virtual void draw(DisplayBuffer* display_buffer,
                    const ScratchItem& item) override {
    display_buffer->print(item.x, item.y, item.font, item.color, item.align,
                          item.text.c_str());
  }

  ScratchItem render_text(Scratch& parent_scratch, const std::string& text,
                          Font* font = nullptr) {
    ScratchItem item = parent_scratch.create(this);
    item.text = text;
    auto color = item.color;
    auto align = item.align;
    if (font) item.font = font;
    print(item, parent_scratch.display_buffer, item.font, color, align,
          text.c_str());
    return item;
  }
};

class AlertAgoWidget : public TextBasedWidget {
 protected:
  std::string format;

 public:
  AlertAgoWidget(const std::string format) : format(format) {}

  static std::string format_up_time(int seconds) {
    int days = seconds / (24 * 3600);
    seconds = seconds % (24 * 3600);
    int hours = seconds / 3600;
    seconds = seconds % 3600;
    int minutes = seconds / 60;
    seconds = seconds % 60;
    if (days) {
      return std::to_string(days) + "d " + std::to_string(hours) + "h " +
             std::to_string(minutes) + "m " + std::to_string(seconds) + "s";
    } else if (hours) {
      return std::to_string(hours) + "h " + std::to_string(minutes) + "m " +
             std::to_string(seconds) + "s";
    } else if (minutes) {
      return std::to_string(minutes) + "m " + std::to_string(seconds) + "s";
    } else {
      return std::to_string(seconds) + "s";
    }
  }

  virtual ScratchItem render(Scratch& parent_scratch) override {
    auto millis_ago = parent_scratch.age_alert_in_millis();
    std::string str;
    if (millis_ago.has_value()) {
      str = format_up_time(millis_ago.value() / 1000);
    } else
      str = "N/A";

    constexpr int MAX_SCRATCH_LENGTH = 200;
    char scratch_buffer[MAX_SCRATCH_LENGTH];
    snprintf(scratch_buffer, MAX_SCRATCH_LENGTH, format.c_str(), str.c_str());
    return render_text(parent_scratch, scratch_buffer);
  }
};

class TextWidget : public TextBasedWidget {
 protected:
  std::string text;

 public:
  TextWidget(const std::string text) : text(text) {}
  void set_text(const std::string& value) { this->text = value; }

  virtual ScratchItem render(Scratch& parent_scratch) override {
    return render_text(parent_scratch, this->text);
  }
};

class MdiWidget : public TextWidget {
 public:
  MdiWidget(const std::string& code) : TextWidget(code) { set_font(font); }

  virtual ScratchItem render(Scratch& parent_scratch) override {
    return render_text(parent_scratch, this->text,
                       parent_scratch.mdi->get_default_font());
  }
};

class AliasMdiWidget : public TextWidget {
 public:
  AliasMdiWidget(const std::string& code) : TextWidget(code) { set_font(font); }

  virtual ScratchItem render(Scratch& parent_scratch) override {
    auto code = parent_scratch.get_mdi()->map_alias(this->text);
    return render_text(parent_scratch, code,
                       parent_scratch.mdi->get_default_font());
  }
};

class AliasMdiAnimationWidget : public TextWidget {
  std::vector<std::unique_ptr<Widget>> widgets;
  int delay;

 public:
  AliasMdiAnimationWidget(int delay, const std::vector<std::string>& aliases)
      : TextWidget("[animation]"), delay(delay) {
    for (auto alias : aliases) {
      widgets.push_back(std::move(std::make_unique<AliasMdiWidget>(alias)));
    }
  }

  virtual ScratchItem render(Scratch& parent_scratch) override {
    auto size = widgets.size();
    if (!size) {
      return render_text(parent_scratch, "[undefined animation]");
    }
    auto index = parent_scratch.get_delay_index(delay) % size;
    const auto& widget = widgets[index];
    return widget->render(parent_scratch);
  }
};

class OnOffStateWidget : public Widget {
 protected:
  std::unique_ptr<Widget> on =
      std::make_unique<AliasMdiWidget>("toggle-switch");
  std::unique_ptr<Widget> off =
      std::make_unique<AliasMdiWidget>("toggle-switch-off");

 public:
  void set_device_class(const std::string value) {
    // https://developers.home-assistant.io/docs/core/entity/binary-sensor/#available-device-classes
    if (value == "motion") {
      on = std::make_unique<AliasMdiAnimationWidget>(
          50, std::vector<std::string>({"run", "walk"}));
      off = std::make_unique<AliasMdiWidget>("walk");
    }
  }
  virtual bool get_render_state() const = 0;

  virtual ScratchItem render(Scratch& parent_scratch) override {
    return (get_render_state() ? on : off)->render(parent_scratch);
  }

  virtual void draw(DisplayBuffer* display_buffer,
                    const ScratchItem& item) override {
    // should not be called ?
  }
};

}  // namespace bmatrix
}  // namespace esphome