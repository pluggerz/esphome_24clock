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

void setup() {
  Leds::set_blink_delay(50);
  Leds::blink(LedColors::green);

  gate.setup();

  rx.setup();
  rx.begin();

  tx.setup();
  tx.begin();
}

void loop() {
  if (Serial.available()) {
    auto value = Serial.read();
    switch (value) {
      case 'p':
        Leds::blink(LedColors::purple);
        break;

      case 'r':
        Leds::blink(LedColors::red);
        break;

      case 'b':
        Leds::blink(LedColors::blue);
        break;

      default:
        Leds::blink(LedColors::white);
        break;
    }
    spit_info('&', value);
  }
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

        Serial.begin(cmd.accept.baudrate);
        gate.start_receiving();

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
  }
}