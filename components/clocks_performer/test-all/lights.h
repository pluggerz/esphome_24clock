#include "../clocks_shared/channel.h"
#include "../clocks_shared/channel.interop.h"
#include "../clocks_shared/lighting.h"

class LightController {
 public:
  lighting::LightingMode current_lighting_mode = -1;
  Micros t0 = 0;

  void process_message(channel::Message *msg);
  void loop();
  void setup();
} extern light_controller;