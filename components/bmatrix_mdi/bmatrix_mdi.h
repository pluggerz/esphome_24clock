#pragma once

#include <map>

#include "esphome/components/display/display_buffer.h"

namespace esphome {
namespace bmatrix {

using esphome::display::Font;

class Mdi {
  const char* const TAG = "Mdi";

  Font* font;
  std::map<std::string, std::string> glyph_map;
  std::map<int, Font*> fonts_by_size;
  int max_size = 0;
  int default_size = 0;

 public:
  Font* get_font_by_size(int size) const {
    for (int idx = size; idx > 0; idx--) {
      if (auto search = this->fonts_by_size.find(idx);
          search != this->fonts_by_size.end())
        return search->second;
    }
    for (int idx = size + 1; idx <= max_size; idx++)
      if (auto search = this->fonts_by_size.find(idx);
          search != this->fonts_by_size.end())
        return search->second;
    return nullptr;
  }

  std::string map_alias(const std::string& value) const {
    auto res = glyph_map.find(value);
    if (res == glyph_map.end()) {
      ESP_LOGW(TAG, "Unable to map: '%s'", value.c_str());
      auto default_value = "help";
      res = glyph_map.find(default_value);
      if (res == glyph_map.end()) {
        ESP_LOGW(TAG, "Unable to map the default: '%s'", default_value);
      }
    }
    return res == glyph_map.end() ? "?" : res->second;
  }

  Font* get_default_font() const { return get_font_by_size(default_size); }

  void set_default_size(int value) { this->default_size = value; }

  void register_font(int size, Font* value) {
    max_size = std::max(size, max_size);
    this->fonts_by_size[size] = value;
  }
  void register_glyph_mapping(const std::string& alias,
                              const std::string& code) {
    this->glyph_map[alias] = code;
  }
};
}  // namespace bmatrix
}  // namespace esphome