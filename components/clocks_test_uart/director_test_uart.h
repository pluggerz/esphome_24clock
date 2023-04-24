#include "esphome/core/component.h"

namespace clock24 {
class DirectorTestUart : public esphome::Component {
 public:
  int baudrate = 0;
  void set_baudrate(int value) { this->baudrate = value; }

  void setup() override;
  void loop() override;

  void kill() {}

  void onewire_init();
  void channel_test_message();
  void channel_write_byte(char value);
};
};  // namespace clock24