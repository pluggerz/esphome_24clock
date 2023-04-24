#include <EEPROM.h>
#define EEPROM_ACCEPTED 0x35

#include "../clocks_shared/channel.h"
#include "../clocks_shared/channel.interop.h"
#include "../clocks_shared/onewire.h"
#include "../clocks_shared/onewire.interop.h"
#include "../clocks_shared/onewire.rx.h"
#include "../clocks_shared/onewire.tx.h"
#include "../common/leds.h"

using channel::ChannelInterop;
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

class PerformerChannel : public rs485::BufferChannel {
 public:
  void process(const byte *bytes, const byte length) override {
    channel::Message *msg = (channel::Message *)bytes;
    spit_info('$', msg->getMsgEnum());
    switch (msg->getMsgEnum()) {
      case channel::MsgEnum::MSG_DUMP_PERFORMERS_VIA_CHANNEL:
        Leds::blink(LedColors::orange);
        break;

      case channel::MsgEnum::MSG_KILL_KEYS_OR_REQUEST_POSITIONS:
        Leds::blink(LedColors::green);
        break;

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
        Leds::blink(LedColors::green, cmd.msg.cmd);
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
        // transmit_ticks();
        break;

      default:
        // just forward
        transmit(cmd);
        break;
    }
  }
}

void setup() {
  Leds::set_blink_delay(50);
  Leds::blink(LedColors::green);

  my_channel.setup();
  gate.setup();

  rx.setup();
  rx.begin();

  tx.setup();
  tx.begin();

  set_action(&reset_action);
}

bool Protocol::is_skippable_message(byte first_byte) { return false; }

void loop() {
  if (Serial.available()) {
    auto value = Serial.peek();
    spit_info('&', value);
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