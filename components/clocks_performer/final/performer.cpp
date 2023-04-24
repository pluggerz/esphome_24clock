#include <EEPROM.h>
#define EEPROM_ACCEPTED 0x34

#include "../clocks_shared/channel.h"
#include "../clocks_shared/channel.interop.h"
#include "../clocks_shared/lighting.h"
#include "../clocks_shared/onewire.interop.h"
#include "../clocks_shared/onewire.rx.h"
#include "../clocks_shared/onewire.tx.h"
#include "../clocks_shared/pins.h"
#include "../clocks_shared/stub.h"
#include "../clocks_shared/ticks.h"
#include "common/keys.executor.h"
#include "common/leds.background.h"
#include "common/leds.h"
#include "common/stepper.h"
#include "common/stepper.util.h"

// note nice, shoulde be removed...
void onewire::RxOnewire::debug(onewire::Value value) {}

using channel::ChannelInterop;
using channel::Message;
using channel::messages::PerformerSettings;
using channel::messages::RequestPositions;
using channel::messages::TickMessage;
using channel::messages::UartEndKeysMessage;
using channel::messages::UartKeysMessage;
using onewire::CmdEnum;
using onewire::OneCommand;
using onewire::OnewireInterrupt;
using rs485::Protocol;

int performer_puid = -1;

uint8_t ChannelInterop::id = ChannelInterop::UNDEFINED;

int onewire::my_performer_id() { return ChannelInterop::id; }

bool Protocol::is_skippable_message(byte first_byte) {
  return false;
  if (first_byte == ChannelInterop::ALL_PERFORMERS) {
    return false;
  }
  return (first_byte >> 1) != ChannelInterop::id;
}

// steppers
int guid_of_director = -1;
lighting::LightingMode current_lighting_mode = -1;

// OneWireProtocol protocol;
onewire::RxOnewire rx;
onewire::TxOnewire tx;

rs485::Gate gate;

class PerformerChannel : public rs485::BufferChannel {
 public:
  void process(const byte *bytes, const byte length) override;
} my_channel;

Micros t0 = 0;

void reset_mode() {}

typedef void (*LoopFunction)();
typedef void (*ListenFunction)();

void transmit(const onewire::OneCommand &value) { tx.transmit(value.raw); }

void spit_info_impl(char ch, int value) {
  transmit(onewire::OneCommand::CheckPoint::for_info(ch, value));
}

void spit_debug_impl(char ch, int value) {
  transmit(onewire::OneCommand::CheckPoint::for_debug(ch, value));
}

void set_action(Action *action);

// TODO: Remove ?
void show_action(const onewire::OneCommand &cmd) {
  switch (cmd.msg.cmd) {
    case CmdEnum::PERFORMER_ONLINE:
      break;

    case CmdEnum::DIRECTOR_ACCEPT:
      break;

    case CmdEnum::PERFORMER_PREPPING:
      break;

    case CmdEnum::DIRECTOR_PING:
      break;

    default:
      break;
  }
  Leds::publish();
}

class DefaultAction : public IntervalAction {
 public:
  virtual void update() override {
    // transmit(OneCommand::busy());
  }

  virtual void loop() override;
} default_action;

class ResetAction : public IntervalAction {
 public:
  virtual void update() override { transmit(OneCommand::performer_online()); }

  virtual void loop() override;
} reset_action;

void execute_director_online(const OneCommand &cmd) {
  // make sure, we only deal once with this id
  int new_guid_of_director = cmd.msg.reserved;
  if (guid_of_director == new_guid_of_director) {
    // ignore message, we are already dealt with it.
    // maybe next one failed to use it ? Lets forward...
    transmit(cmd.increase_performer_id_and_forward());
    return;
  }
  ChannelInterop::id = ChannelInterop::UNDEFINED;
  guid_of_director = new_guid_of_director;

  // go to reset mode
  set_action(&reset_action);

  // inform next
  transmit(cmd.increase_performer_id_and_forward());
}

void dump_performer(int source) {
  transmit(onewire::OneCommand::CheckPoint::for_info('D', source));
  transmit(onewire::OneCommand::CheckPoint::for_info(
      '@', my_channel.baudrate() / 100));
  transmit(onewire::OneCommand::CheckPoint::for_info('U', millis() / 1000));
  if (my_channel.error_count()) {
    transmit(onewire::OneCommand::CheckPoint::for_info(
        'C', my_channel.error_count()));
  }
  if (my_channel.skip_count()) {
    transmit(onewire::OneCommand::CheckPoint::for_info(
        'S', my_channel.skip_count()));
  }
  if (rx.error_count()) {
    transmit(onewire::OneCommand::CheckPoint::for_info('O', rx.error_count()));
    transmit(onewire::OneCommand::CheckPoint::for_info(
        'E', rx.rx_start_detected_after_data));
    transmit(onewire::OneCommand::CheckPoint::for_info(
        'L', rx.rx_too_long_empty_detected_after_data));
  }
}

void to_debug() {
  lighting::current = &lighting::debug;
  lighting::current->start();
  lighting::current->update(millis());
  lighting::current->publish();
}
void kill_steppers() { step_executors.kill(); }

void DefaultAction::loop() {
  // my_channel.loop();

  IntervalAction::loop();

  Leds::set_ex(LED_MODE, LedColors::orange);

  if (!rx.pending()) return;

  OneCommand cmd;
  cmd.raw = rx.flush();
  if (!cmd.parity_okay()) {
    return;
  }
  show_action(cmd);

  switch (cmd.msg.cmd) {
    case CmdEnum::TO_DEBUG:
      to_debug();
      transmit(cmd);
      break;
    case CmdEnum::KILL_STEPPERS:
      kill_steppers();
      transmit(cmd);
      break;

    case CmdEnum::DUMP_PERFORMERS:
      dump_performer(1);
      transmit(onewire::command_builder.dump_performers_by_performer());
      break;

    // these are special, we need to adapt the source
    case CmdEnum::DIRECTOR_ACCEPT:
      transmit(cmd.increase_performer_id_and_forward());
      break;

    case CmdEnum::DIRECTOR_ONLINE:
      current_lighting_mode = -1;
      lighting::current = &lighting::debug;
      execute_director_online(cmd);
      break;

    case CmdEnum::DIRECTOR_POSITION_ACK:
      if (cmd.position_ack.puid == performer_puid)
        transmit(cmd);
      else {
        transmit(
            onewire::OneCommand::CheckPoint::for_info('<', performer_puid));
        transmit(onewire::OneCommand::CheckPoint::for_info(
            '>', cmd.position_ack.puid));
      }
      break;

      // these will be forwarded, without any changes (also default action):
    case CmdEnum::TOCK:
    case CmdEnum::DIRECTOR_PING:
    case CmdEnum::PERFORMER_POSITION:
    case CmdEnum::PERFORMER_ONLINE:
    default:
      transmit(cmd);
      break;
  }
}

void transmit_ticks() {
  auto ticks0 = Ticks::normalize(stepper0.ticks()) / STEP_MULTIPLIER;
  auto ticks1 = Ticks::normalize(stepper1.ticks()) / STEP_MULTIPLIER;

  transmit(OneCommand::Position::create(ticks0, ticks1));
}

void StepExecutors::send_positions() {
  transmit(onewire::OneCommand::CheckPoint::for_info('P', 0));
  transmit_ticks();
}

void ResetAction::loop() {
  IntervalAction::loop();

  Leds::set_ex(LED_MODE, LedColors::blue);

  if (rx.pending()) {
    onewire::OneCommand cmd;
    cmd.raw = rx.flush();
    if (!cmd.parity_okay()) {
      return;
    }
    show_action(cmd);
    switch (cmd.msg.cmd) {
      case CmdEnum::TO_DEBUG:
        to_debug();
        transmit(cmd);
        break;
      case CmdEnum::KILL_STEPPERS:
        kill_steppers();
        transmit(cmd);
        break;

      case CmdEnum::DUMP_PERFORMERS:
        dump_performer(2);
        transmit(onewire::command_builder.dump_performers_by_performer());
        break;

      case CmdEnum::DIRECTOR_ONLINE:
        execute_director_online(cmd);
        break;

      case CmdEnum::PERFORMER_ONLINE:
        // we are already waiting, so ignore this one
        break;

      case CmdEnum::DIRECTOR_ACCEPT:
        ChannelInterop::id = cmd.derive_next_source_id();
        spit_info('@', 1);
        if (my_channel.baudrate() != cmd.accept.baudrate) {
          my_channel.begin(cmd.accept.baudrate);
          my_channel.start_receiving();
          spit_info('@', 2);
        } else {
          my_channel.start_receiving();

          set_action(&default_action);

          // send after we 'heard' twice the right baudrate
          transmit(cmd.increase_performer_id_and_forward());
          // transmit_ticks();
          spit_info('@', 3);
        }
        break;

      default:
        // just forward
        // transmit(cmd.forward());
        break;
    }
  }
}

Action *current_action = nullptr;

Micros t0_cmd;
void start_reset_cmd() { t0_cmd = millis(); }

void set_action(Action *action) {
  if (current_action != nullptr) current_action->final();
  current_action = action;
  if (current_action != nullptr) current_action->setup();
}

void setup() {
  bool accepted = EEPROM.read(0) == EEPROM_ACCEPTED;
  EEPROM.write(0, 0);

  Leds::set_blink_delay(accepted ? 20 : 500);
  Leds::blink(accepted ? LedColors::green : LedColors::red);

  StepperUtil::setup_steppers();
  Leds::blink(LedColors::orange);

  StepperUtil::calibrate_steppers();
  StepExecutors::setup(stepper0, stepper1);

  Leds::blink(LedColors::blue);

  pinMode(SLAVE_RS485_TXD_DPIN, OUTPUT);
  pinMode(SLAVE_RS485_RXD_DPIN, INPUT);

  Leds::setup();
  Leds::blink(LedColors::green, 1);

  rx.setup();
#ifdef DOLED
  Leds::set_ex(LED_ONEWIRE, LedColors::purple);
#endif
  rx.begin();
  Leds::blink(LedColors::green, 2);

  tx.setup();
  tx.begin();

  Leds::blink(LedColors::green, 3);
  my_channel.setup();

#ifndef APA102_USE_FAST_GPIO
  Leds::error(LEDS_ERROR_MISSING_APA102_USE_FAST_GPIO);
#endif

  set_action(&reset_action);
  lighting::current->start();
}

LoopFunction current = reset_mode;

void loop() {
  // for debugging
  Millis now = millis();
  if (now - t0 > 50) {
    t0 = now;
    Leds::publish();
    if (lighting::current->update(millis())) {
      lighting::current->publish();
    }
  }

  if (Serial.available()) {
    auto value = Serial.peek();
    spit_info('&', value);
  }
  my_channel.loop();
  if (current_action) current_action->loop();
}

void execute_performer_settings(const PerformerSettings *settings) {
  if (settings->get_performer_destination_id() != ChannelInterop::id) return;
  stepper0.set_offset_steps(settings->magnet_offet0);
  stepper1.set_offset_steps(settings->magnet_offet1);
  Leds::blink(LedColors::purple, 1 + ChannelInterop::id);
}

void process_individual_lighting(channel::messages::IndividualLeds *msg) {
  if (msg->get_performer_destination_id() != ChannelInterop::id) {
    return;
  }
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
      if (mode == current_lighting_mode) return;

      lighting::current = &lighting::xmas;
      lighting::xmas.setPattern(mode);
      transmit(onewire::OneCommand::CheckPoint::for_info('l', 1));
      break;

    case lighting::Rainbow:
      if (mode == current_lighting_mode) return;

      lighting::current = &lighting::rainbow;
      transmit(onewire::OneCommand::CheckPoint::for_info('l', 2));
      break;

    case lighting::Off:
      lighting::solid.color = rgb_color(0, 0, 0);
      lighting::current = &lighting::solid;
      transmit(onewire::OneCommand::CheckPoint::for_info('l', 3));
      break;

    case lighting::Solid:
      lighting::solid.color = rgb_color(msg->r, msg->g, msg->b);
      lighting::current = &lighting::solid;
      transmit(onewire::OneCommand::CheckPoint::for_info('l', 4));
      break;

    case lighting::Debug:
      lighting::current = &lighting::debug;
      transmit(onewire::OneCommand::CheckPoint::for_info('l', 5));
      break;

    case lighting::Individual:
      lighting::current = &lighting::individual;
      transmit(onewire::OneCommand::CheckPoint::for_info('l', 6));
      break;

    default:
      transmit(onewire::OneCommand::CheckPoint::for_info('l', 7));
      break;
  };
  lighting::current->start();
  lighting::current->update(millis());
  lighting::current->publish();
}

void PerformerChannel::process(const byte *bytes, const byte length) {
  channel::Message *msg = (channel::Message *)bytes;
  spit_debug('$', msg->getMsgEnum());

  switch (msg->getMsgEnum()) {
    case channel::MsgEnum::MSG_DUMP_PERFORMERS_VIA_CHANNEL:
      dump_performer(3);
      return;

    case channel::MsgEnum::MSG_BEGIN_KEYS:
      step_executors.process_begin_keys(msg);
      return;

    case channel::MsgEnum::MSG_SEND_KEYS: {
      auto performer_id = msg->get_performer_destination_id();
      if (performer_id == ChannelInterop::id) {
        step_executors.process_add_keys(
            reinterpret_cast<const UartKeysMessage *>(msg));
      }
    }
      return;

    case channel::MsgEnum::MSG_END_KEYS:
      step_executors.process_end_keys(
          ChannelInterop::id << 1,
          reinterpret_cast<const UartEndKeysMessage *>(msg));

      return;

    case channel::MsgEnum::MSG_KILL_KEYS_OR_REQUEST_POSITIONS:
      // this message is
      if (ChannelInterop::id != ChannelInterop::UNDEFINED) {
        if (step_executors.active()) {
          step_executors.request_stop();
          performer_puid = -1;
          transmit(onewire::OneCommand::CheckPoint::for_info('(', 0));
        } else {
          transmit_ticks();
          performer_puid =
              reinterpret_cast<const RequestPositions *>(msg)->puid;
          transmit(
              onewire::OneCommand::CheckPoint::for_info('?', performer_puid));
        }
      } else {
        transmit(onewire::OneCommand::CheckPoint::for_info(')', 0));
      }
      break;

    case channel::MsgEnum::MSG_REQUEST_POSITIONS:
      if (ChannelInterop::id != ChannelInterop::UNDEFINED) {
        transmit_ticks();
        performer_puid = reinterpret_cast<const RequestPositions *>(msg)->puid;
        transmit(
            onewire::OneCommand::CheckPoint::for_info('?', performer_puid));
      } else {
        transmit(onewire::OneCommand::CheckPoint::for_info(')', 1));
      }
      break;

    case channel::MsgEnum::MSG_PERFORMER_SETTINGS:
      execute_performer_settings(static_cast<PerformerSettings *>(msg));
      rx.reset_error_count();
      EEPROM.update(0, EEPROM_ACCEPTED);
      break;

    case channel::MsgEnum::MSG_TICK: {
      TickMessage *tick = (TickMessage *)bytes;
      if (ChannelInterop::id != ChannelInterop::UNDEFINED)
        transmit(onewire::OneCommand::tock(ChannelInterop::get_my_id(),
                                           tick->value));
    } break;

    case channel::MsgEnum::MSG_LIGTHING_MODE:
      process_lighting(static_cast<channel::messages::LightingMode *>(msg));
      break;

    case channel::MsgEnum::MSG_INDIVIDUAL_LEDS_SET:
      process_individual_lighting(
          static_cast<channel::messages::IndividualLeds *>(msg));
      break;

    case channel::MsgEnum::MSG_INDIVIDUAL_LEDS_SHOW:
      lighting::individual.show();
      break;
  }
}
