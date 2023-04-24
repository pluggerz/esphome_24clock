#include "esphome/core/component.h"

namespace clock24 {
class DirectorOnewireTest : public esphome::Component {
 public:
  int baudrate = 0;
  void set_baudrate(int value) { this->baudrate = value; }

  void setup() override;
  void loop() override;

  void kill() {}

  void channel_write_byte(char value);
  void onewire_ping();
  void onewire_accept();
};
};  // namespace clock24