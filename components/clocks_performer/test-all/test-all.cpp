#include <EEPROM.h>
#define EEPROM_ACCEPTED 0x35

#include "../clocks_shared/channel.h"
#include "../clocks_shared/channel.interop.h"
#include "../clocks_shared/onewire.h"
#include "../clocks_shared/onewire.interop.h"
#include "../clocks_shared/onewire.rx.h"
#include "../clocks_shared/onewire.tx.h"
#include "../clocks_shared/ticks.h"
#include "../common/leds.h"
#include "common/keys.executor.h"
#include "common/stepper.h"
#include "common/stepper.util.h"

using channel::ChannelInterop;
using channel::messages::PerformerSettings;
using channel::messages::RequestPositions;
using channel::messages::TickMessage;
using channel::messages::UartEndKeysMessage;
using channel::messages::UartKeysMessage;
using onewire::CmdEnum;
using onewire::OneCommand;
using rs485::Protocol;

rs485::Gate gate;

// OneWireProtocol protocol;
onewire::RxOnewire rx;
onewire::TxOnewire tx;

uint8_t ChannelInterop::id = ChannelInterop::UNDEFINED;

int onewire::my_performer_id() { return ChannelInterop::id; }

void transmit(const onewire::OneCommand &value) { tx.transmit(value.raw); }

void spit_info_impl(char ch, int value) {
  transmit(onewire::OneCommand::CheckPoint::for_info(ch, value));
}
void spit_debug_impl(char ch, int value) {
  transmit(onewire::OneCommand::CheckPoint::for_info(ch, value));
}

int performer_puid = -1;

void transmit_ticks() {
  auto ticks0 = Ticks::normalize(stepper0.ticks()) / STEP_MULTIPLIER;
  auto ticks1 = Ticks::normalize(stepper1.ticks()) / STEP_MULTIPLIER;

  transmit(OneCommand::Position::create(ticks0, ticks1));
}

void execute_performer_settings(const PerformerSettings *settings) {
  if (settings->get_performer_destination_id() != ChannelInterop::id) return;
  stepper0.set_offset_steps(settings->magnet_offet0);
  stepper1.set_offset_steps(settings->magnet_offet1);
  Leds::blink(LedColors::purple, 1 + ChannelInterop::id);
}

void StepExecutors::send_positions() {
  transmit(onewire::OneCommand::CheckPoint::for_info('P', 0));
  transmit_ticks();
}

class PerformerChannel : public rs485::BufferChannel {
 public:
  void process(const byte *bytes, const byte length) override {
    channel::Message *msg = (channel::Message *)bytes;
    spit_debug('$', msg->getMsgEnum());
    switch (msg->getMsgEnum()) {
      case channel::MsgEnum::MSG_DUMP_PERFORMERS_VIA_CHANNEL:
        Leds::blink(LedColors::orange);
        break;

      case channel::MsgEnum::MSG_BEGIN_KEYS:
        spit_info('k', 0);

        step_executors.process_begin_keys(msg);
        return;

      case channel::MsgEnum::MSG_SEND_KEYS: {
        spit_info('k', 1);

        auto performer_id = msg->get_performer_destination_id();
        if (performer_id == ChannelInterop::id) {
          step_executors.process_add_keys(
              reinterpret_cast<const UartKeysMessage *>(msg));
        }
      }
        return;

      case channel::MsgEnum::MSG_END_KEYS:
        spit_info('k', 2);

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
          performer_puid =
              reinterpret_cast<const RequestPositions *>(msg)->puid;
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

      default:
        // Leds::blink(LedColors::blue);
        break;
    }
  }
} my_channel;

Action *current_action = nullptr;

void set_action(Action *action) {
  if (current_action != nullptr) current_action->final();
  current_action = action;
  if (current_action != nullptr) current_action->setup();
}

class ResetAction : public IntervalAction {
 public:
  virtual void update() override { transmit(OneCommand::performer_online()); }

  virtual void loop() override;
} reset_action;

class DefaultAction : public IntervalAction {
 public:
  virtual void update() override {
    // transmit(OneCommand::busy());
  }

  virtual void loop() override;
} default_action;

int guid_of_director = -1;

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
  switch (cmd.msg.cmd) {
    case CmdEnum::TO_DEBUG:
      transmit(cmd);
      break;
    case CmdEnum::KILL_STEPPERS:
      transmit(cmd);
      break;

    case CmdEnum::DUMP_PERFORMERS:
      transmit(onewire::command_builder.dump_performers_by_performer());
      break;

    // these are special, we need to adapt the source
    case CmdEnum::DIRECTOR_ACCEPT:
      transmit(cmd.increase_performer_id_and_forward());
      break;

    case CmdEnum::DIRECTOR_ONLINE:
      execute_director_online(cmd);
      break;

    case CmdEnum::DIRECTOR_POSITION_ACK:
      if (cmd.position_ack.puid == performer_puid)
        transmit(cmd);
      else {
        spit_info('<', performer_puid);
        spit_info('>', cmd.position_ack.puid);
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

void ResetAction::loop() {
  IntervalAction::loop();

  Leds::set_ex(LED_MODE, LedColors::blue);

  if (rx.pending()) {
    onewire::OneCommand cmd;
    cmd.raw = rx.flush();
    if (!cmd.parity_okay()) {
      return;
    }
    // show_action(cmd);
    switch (cmd.msg.cmd) {
      case CmdEnum::TO_DEBUG:
        // to_debug();
        transmit(cmd);
        break;
      case CmdEnum::KILL_STEPPERS:
        // kill_steppers();
        transmit(cmd);
        break;

      case CmdEnum::DUMP_PERFORMERS:
        // dump_performer(2);
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
        my_channel.begin(cmd.accept.baudrate);
        my_channel.start_receiving();
        transmit(cmd.increase_performer_id_and_forward());

        // send after we 'heard' twice the right baudrate
        transmit(cmd.increase_performer_id_and_forward());
        set_action(&default_action);
        break;

      default:
        // just forward
        transmit(cmd);
        break;
    }
  }
}

void setup() {
  bool accepted = EEPROM.read(0) == EEPROM_ACCEPTED;
  EEPROM.write(0, 0);

  Leds::set_blink_delay(accepted ? 20 : 100);
  Leds::blink(accepted ? LedColors::green : LedColors::red);

  StepperUtil::setup_steppers();
  Leds::blink(LedColors::orange);

  StepperUtil::calibrate_steppers();
  StepExecutors::setup(stepper0, stepper1);
  Leds::blink(LedColors::blue);

  my_channel.setup();
  gate.setup();

  rx.setup();
  rx.begin();

  tx.setup();
  tx.begin();

  set_action(&reset_action);
}

bool Protocol::is_skippable_message(byte first_byte) {
  return false;
  if (first_byte == ChannelInterop::ALL_PERFORMERS) {
    return false;
  }
  return (first_byte >> 1) != ChannelInterop::id;
}

void loop() {
  if (Serial.available()) {
    auto value = Serial.peek();
    spit_debug('&', value);
  }
  my_channel.loop();
  if (current_action) current_action->loop();
  /*
    if (rx.pending()) {
      OneCommand cmd;
      cmd.raw = rx.flush();
      if (!cmd.parity_okay()) {
        Leds::blink(LedColors::red);
        return;
      }
      switch (cmd.msg.cmd) {
        case CmdEnum::DUMP_PERFORMERS:
          Leds::blink(LedColors::red, cmd.msg.cmd);
          transmit(cmd.increase_performer_id_and_forward());
          break;

        case CmdEnum::DIRECTOR_ONLINE:
          Leds::blink(LedColors::green, cmd.msg.cmd);
          transmit(cmd.increase_performer_id_and_forward());
          break;

        case CmdEnum::DIRECTOR_ACCEPT:
          ChannelInterop::id = cmd.derive_next_source_id();

          Leds::blink(LedColors::orange, cmd.msg.cmd);

          my_channel.begin(cmd.accept.baudrate);
          my_channel.start_receiving();

          transmit(cmd.increase_performer_id_and_forward());
          break;

        case CmdEnum::DIRECTOR_PING:
          Leds::blink(LedColors::blue, cmd.msg.cmd);
          transmit(cmd);
          break;

        default:
          // forward
          // Leds::blink(LedColors::purple, cmd.msg.cmd);
          transmit(cmd);
          break;
      }
    }*/
}