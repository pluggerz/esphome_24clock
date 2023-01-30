#pragma once

#include "bmatrix_group.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"

namespace esphome {
namespace bmatrix {

template <typename... Ts>
class AlertOnAction : public Action<Ts...> {
 public:
  explicit AlertOnAction(Group* a_group, BMatrix* matrix)
      : a_group(a_group), matrix(matrix) {}

  void play(Ts... x) override { matrix->add_alert(this->a_group); }

 protected:
  Group* a_group;
  BMatrix* matrix;
};

template <typename... Ts>
class AlertOffAction : public Action<Ts...> {
 public:
  explicit AlertOffAction(Group* a_group, BMatrix* matrix)
      : a_group(a_group), matrix(matrix) {}

  void play(Ts... x) override { matrix->remove_alert(this->a_group); }

 protected:
  Group* a_group;
  BMatrix* matrix;
};

}  // namespace bmatrix
}  // namespace esphome