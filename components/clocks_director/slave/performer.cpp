#include "../clocks_shared/pins.h"
#include "../clocks_shared/stub.h"
#include "../clocks_shared/ticks.h"
#include "channel.h"
#include "channel.interop.h"
#include "keys.executor.h"
#include "leds.h"
#include "onewire.h"
#include "onewire.interop.h"
#include "stepper.h"

using channel::ChannelInterop;
using channel::Message;
using channel::messages::UartEndKeysMessage;
using channel::messages::UartKeysMessage;
using onewire::CmdEnum;
using onewire::OneCommand;

uint8_t ChannelInterop::id = ChannelInterop::UNDEFINED;

int onewire::my_performer_id() { return ChannelInterop::id; }

// steppers
int guid_of_director = -1;

Stepper0 stepper0(NUMBER_OF_STEPS);
Stepper1 stepper1(NUMBER_OF_STEPS);

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

void set_action(Action *action);

void show_action(const onewire::OneCommand &cmd) {
  switch (cmd.msg.cmd) {
    case CmdEnum::PERFORMER_ONLINE:
      Leds::set(LED_COUNT - 5, rgb_color(0x00, 0x00, 0xFF));
      break;

    case CmdEnum::DIRECTOR_ACCEPT:
      Leds::set(LED_COUNT - 5, rgb_color(0x00, 0xFF, 0x00));
      break;

    case CmdEnum::PERFORMER_PREPPING:
      Leds::set(LED_COUNT - 5, rgb_color(0x00, 0xFF, 0xFF));
      break;

    case CmdEnum::DIRECTOR_PING:
      Leds::set(LED_COUNT - 5, rgb_color(0xFF, 0xFF, 0x00));
      break;

    default:
      Leds::set(LED_COUNT - 5, rgb_color(0xFF, 0x00, 0x00));
      break;
  }
  Leds::publish();
}
#if MODE >= MODE_ONEWIRE_INTERACT

class DefaultAction : public IntervalAction {
 public:
  virtual void update() override {
    // transmit(OneCommand::busy());
  }

  virtual void setup() override {
    IntervalAction::setup();
    Leds::set(LED_COUNT - 2, rgb_color(0x00, 0xFF, 0x00));
  }

  virtual void loop() override;
} default_action;

class ResetAction : public IntervalAction {
 public:
  virtual void update() override { transmit(OneCommand::performer_online()); }

  virtual void setup() override {
    IntervalAction::setup();
    Leds::set(LED_COUNT - 2, rgb_color(0xFF, 0x00, 0x00));
  }

  virtual void loop() override;
} reset_action;

void execute_director_online(const OneCommand &cmd) {
  // make sure, we only deal once with this id
  int new_guid_of_director = cmd.msg.reserved;
  if (guid_of_director == new_guid_of_director) {
    // ignore message, we are already dealt with it.
    // maybe next one failed to use it ? Lets forward...
    transmit(cmd.forward());
    return;
  }
  ChannelInterop::id = ChannelInterop::UNDEFINED;
  guid_of_director = new_guid_of_director;

  // align onewire channel
  onewire::OnewireInterrupt::align();

  // go to reset mode
  set_action(&reset_action);

  // inform next
  transmit(cmd.forward());
}

void DefaultAction::loop() {
  IntervalAction::loop();

  Leds::set_ex(LED_MODE, LedColors::orange);

  if (!rx.pending()) return;

  OneCommand cmd;
  cmd.raw = rx.flush();
  show_action(cmd);
  my_channel.loop();
  switch (cmd.msg.cmd) {
    // these are special, we need to adapt the source
    case CmdEnum::DIRECTOR_ACCEPT:
      transmit(cmd.forward());
      break;

    case CmdEnum::DIRECTOR_ONLINE:
      execute_director_online(cmd);
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

  transmit(OneCommand::Position::create(ChannelInterop::get_my_id(), ticks0,
                                        ticks1));
}

void ResetAction::loop() {
  IntervalAction::loop();

  Leds::set_ex(LED_MODE, LedColors::blue);

  // Leds::set(7, rgb_color(0xFF, 0xFF, 0x00));

  if (rx.pending()) {
    onewire::OneCommand cmd;
    cmd.raw = rx.flush();
    show_action(cmd);
    switch (cmd.msg.cmd) {
      case CmdEnum::DIRECTOR_ONLINE:
        execute_director_online(cmd);
        break;

      case CmdEnum::PERFORMER_ONLINE:
        // we are already waiting, so ignore this one
        break;

      case CmdEnum::DIRECTOR_ACCEPT:
        ChannelInterop::id = cmd.derive_next_source_id();
        set_action(&default_action);
        if (my_channel.baudrate() != cmd.accept.baudrate) {
          my_channel.baudrate(cmd.accept.baudrate);
          my_channel.start_receiving();
        }
        transmit(cmd.forward());
        transmit_ticks();
        break;

      default:
        // just forward
        // transmit(cmd.forward());
        break;
    }
  }
}

#endif

Action *current_action = nullptr;

Micros t0_cmd;
void start_reset_cmd() { t0_cmd = millis(); }

void set_action(Action *action) {
  if (current_action != nullptr) current_action->final();
  current_action = action;
  if (current_action != nullptr) current_action->setup();
}

void setup_steppers() {
  // set mode
  pinMode(MOTOR_SLEEP_PIN, OUTPUT);
  pinMode(MOTOR_ENABLE, OUTPUT);
  pinMode(MOTOR_RESET, OUTPUT);

  stepper0.setup();
  stepper1.setup();

  delay(10);

  // set state
  digitalWrite(MOTOR_SLEEP_PIN, LOW);
  digitalWrite(MOTOR_ENABLE, HIGH);
  digitalWrite(MOTOR_RESET, LOW);

  // wait at least 1ms
  delay(10);
  digitalWrite(MOTOR_SLEEP_PIN, HIGH);
  digitalWrite(MOTOR_ENABLE, LOW);
  digitalWrite(MOTOR_RESET, HIGH);

  // wait at least 200ns
  delay(10);
}

void setup() {
  setup_steppers();

  pinMode(SLAVE_RS485_TXD_DPIN, OUTPUT);
  pinMode(SLAVE_RS485_RXD_DPIN, INPUT);

  Leds::setup();
  Leds::blink(LedColors::green, 1);

  rx.setup();
#if MODE != MODE_ONEWIRE_PASSTROUGH
#ifdef DOLED
  Leds::set_ex(LED_ONEWIRE, LedColors::purple);
#endif
  rx.begin();
  Leds::blink(LedColors::green, 2);
#endif

  tx.setup();
#if MODE != MODE_ONEWIRE_PASSTROUGH
  tx.begin();
#endif
  Leds::blink(LedColors::green, 3);
  my_channel.setup();

#if MODE == MODE_CHANNEL || MODE == MODE_ONEWIRE_CMD || \
    MODE == MODE_ONEWIRE_PASSTROUGH
  channel.baudrate(9600);
  channel.start_receiving();
  Leds::blink(LedColors::green, 4);
#endif

  gate.setup();
  gate.start_receiving();
  Leds::blink(LedColors::green, 5);

#ifndef APA102_USE_FAST_GPIO
  Leds::error(LEDS_ERROR_MISSING_APA102_USE_FAST_GPIO);
#endif

#if MODE >= MODE_ONEWIRE_INTERACT
  set_action(&reset_action);

  int speed = 10;
  stepper0.set_speed_in_revs_per_minute(speed);
  stepper1.set_speed_in_revs_per_minute(speed);
  while (!stepper0.find_magnet_tick() || !stepper1.find_magnet_tick()) {
  }
  for (int idx = 0; idx < 240; ++idx) {
    stepper0.step(-1);
    stepper1.step(-1);
  }

  stepper0.set_speed_in_revs_per_minute(speed);
  stepper1.set_speed_in_revs_per_minute(speed);
  while (!stepper0.find_magnet_tick() || !stepper1.find_magnet_tick()) {
  }
  for (int idx = 0; idx < 240; ++idx) {
    stepper0.step(-1);
    stepper1.step(-1);
  }
  StepExecutors::setup(stepper0, stepper1);
#endif
}

LoopFunction current = reset_mode;

#if MODE == MODE_ONEWIRE_PASSTROUGH

namespace Hal {
void yield() {}
}  // namespace Hal

void loop() {
  // for debugging
  Millis now = millis();
  if (now - t0 > 25) {
    t0 = now;
    Leds::publish();
  }

  my_channel.loop();
  StepExecutors.loop(now);
}
#endif

#if MODE >= MODE_ONEWIRE_MIRROR

namespace Hal {
void yield() {}
}  // namespace Hal

void loop() {
  // for debugging
  Millis now = millis();
  if (now - t0 > 25) {
    t0 = now;
    Leds::publish();
  }

  my_channel.loop();
#if MODE == MODE_ONEWIRE_INTERACT
  StepExecutors::loop(micros());
#endif

  if (rx.pending()) {
#if MODE == MODE_ONEWIRE_MIRROR
    auto value = rx.flush();
    tx.transmit(value);
#endif
#if MODE == MODE_ONEWIRE_CMD
    onewire::OneCommand cmd;
    cmd.raw = rx.flush();
    tx.transmit(cmd.forward().raw);
#endif
  }
  if (current_action != nullptr) current_action->loop();
}
#endif

void execute_settings(const channel::messages::StepperSettings *settings) {
  if (settings->getDstId() != ChannelInterop::id) return;
  stepper0.set_offset_steps(settings->magnet_offet0);
  stepper1.set_offset_steps(settings->magnet_offet1);
  Leds::blink(LedColors::purple, 1 + ChannelInterop::id);
}

void PerformerChannel::process(const byte *bytes, const byte length) {
  channel::Message *msg = (channel::Message *)bytes;
  switch (msg->getMsgEnum()) {
    case channel::MsgEnum::MSG_BEGIN_KEYS:
      StepExecutors::process_begin_keys(msg);
      return;

    case channel::MsgEnum::MSG_SEND_KEYS: {
      auto performer_id = msg->getDstId() >> 1;
      if (performer_id == ChannelInterop::id) {
        StepExecutors::process_add_keys(
            reinterpret_cast<const UartKeysMessage *>(msg));
      }
    }
      return;

    case channel::MsgEnum::MSG_END_KEYS:
      StepExecutors::process_end_keys(
          ChannelInterop::id << 1,
          reinterpret_cast<const UartEndKeysMessage *>(msg));

      return;

    case channel::MsgEnum::MSG_POSITION_REQUEST:
      if (ChannelInterop::id != ChannelInterop::UNDEFINED) {
        transmit_ticks();
      }
      break;

    case channel::MsgEnum::MSG_PERFORMER_SETTINGS:
      execute_settings(static_cast<channel::messages::StepperSettings *>(msg));
      break;

    case channel::MsgEnum::MSG_TICK: {
      channel::messages::IntMessage *tick =
          (channel::messages::IntMessage *)bytes;
      if (ChannelInterop::id != ChannelInterop::UNDEFINED)
        transmit(onewire::OneCommand::tock(ChannelInterop::get_my_id(),
                                           tick->value));
    } break;

    default:
      // Leds::set_ex(LED_CHANNEL, rgb_color(0xff, 0x00, 0x00));
      // Leds::publish();
      break;
  }
}