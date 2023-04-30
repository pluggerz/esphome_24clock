#include "lights.h"

#include "common/leds.background.h"
#include "common/leds.h"

void process_lighting(channel::messages::LightingMode *msg) {
  auto mode = msg->mode;
  Leds::set_brightness(msg->brightness);

  switch (mode) {
    case lighting::WarmWhiteShimmer:
    case lighting::RandomColorWalk:
    case lighting::TraditionalColors:
    case lighting::ColorExplosion:
    case lighting::Gradient:
    case lighting::BrightTwinkle:
    case lighting::Collision:
      if (mode == light_controller.current_lighting_mode) return;

      lighting::current = &lighting::xmas;
      lighting::xmas.setPattern(mode);
      spit_info('l', 1);
      break;

    case lighting::Rainbow:
      if (mode == light_controller.current_lighting_mode) return;

      lighting::current = &lighting::rainbow;
      spit_info('l', 2);
      break;

    case lighting::Off:
      lighting::solid.color = rgb_color(0, 0, 0);
      lighting::current = &lighting::solid;
      spit_info('l', 3);
      break;

    case lighting::Solid:
      lighting::solid.color = rgb_color(msg->r, msg->g, msg->b);
      lighting::current = &lighting::solid;
      spit_info('l', 4);
      break;

    case lighting::Debug:
      lighting::current = &lighting::debug;
      spit_info('l', 5);
      break;

    case lighting::Individual:
      lighting::current = &lighting::individual;
      spit_info('l', 6);
      break;

    default:
      spit_info('l', 7);
      break;
  };
  lighting::current->start();
  lighting::current->update(millis());
  lighting::current->publish();
}

void process_individual_lighting(channel::messages::IndividualLeds *msg) {
  if (lighting::current != &lighting::individual) {
    return;
  }
  // lighting::current = &lighting::individual;
  for (int led = 0; led < LED_COUNT; ++led) {
    const auto &source = msg->leds[led];
    lighting::individual.set(
        led, rgb_color(source.r << 3, source.g << 2, source.b << 3));
  }
}

void LightController::loop() {
  Millis now = millis();
  if (now - t0 > 25) {
    t0 = now;
    Leds::publish();
    if (lighting::current->update(millis())) {
      lighting::current->publish();
    }
  }
}

void LightController::process_message(channel::Message *msg) {
  switch (msg->getMsgEnum()) {
    case channel::MsgEnum::MSG_LIGTHING_MODE:
      process_lighting(static_cast<channel::messages::LightingMode *>(msg));
      break;

    case channel::MsgEnum::MSG_INDIVIDUAL_LEDS_SET:
      process_individual_lighting(
          static_cast<channel::messages::IndividualLeds *>(msg));
      break;

    case channel::MsgEnum::MSG_INDIVIDUAL_LEDS_SHOW:
      // lighting::individual.show();
      break;
  }
}