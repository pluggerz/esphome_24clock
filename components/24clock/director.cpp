#include "stub.h"

#if defined(IS_DIRECTOR)

#include "esphome/core/helpers.h"
#include "pinio.h"

esphome::HighFrequencyLoopRequester highFrequencyLoopRequester;

#include "esphome/core/log.h"

using namespace esphome;

#include "animation.h"
#include "animation.time.h"
#include "channel.h"
#include "channel.interop.h"
#include "director.h"
#include "pins.h"

using channel::ChannelInterop;
using onewire::CmdEnum;

uint8_t ChannelInterop::id = ChannelInterop::DIRECTOR;
int guid = rand();

static const char *const TAG = "controller";

using clock24::Director;

namespace Hal {
void yield() {}
}  // namespace Hal

Director::Director() {}

onewire::RxOnewire rx;
onewire::TxOnewire tx;

class DirectorChannel : public rs485::BufferChannel {
 public:
  DirectorChannel() { baudrate(9600); }

  virtual void process(const byte *bytes, const byte length) override {}
} my_channel;

constexpr int MAX_SCRATCH_LENGTH = 200;
char scratch_buffer[MAX_SCRATCH_LENGTH];

const char *format(const onewire::OneCommand &cmd) {
  char const *from;
  int id = 0;
  if (cmd.from_master()) {
    from = "D";
    id = 0;
  } else if (cmd.from_performer()) {
    from = "p";
    id = cmd.msg.source_id;
  } else {
    from = "U";
    id = cmd.msg.source_id;
  }

  char const *what;
  switch (cmd.msg.cmd) {
    case CmdEnum::DIRECTOR_ONLINE:
      what = "DIRECTOR_ONLINE";
      break;
    case onewire::TOCK:
      what = "TOCK";
      break;
    case CmdEnum::DIRECTOR_ACCEPT:
      what = "DIRECTOR_ACCEPT";
      break;
    case CmdEnum::PERFORMER_POSITION:
      what = "PERFORMER_POSITION";
      break;
    case CmdEnum::DIRECTOR_PING:
      what = "DIRECTOR_PING";
      break;
    default:
      what = "UNKNOWN";
      break;
  }
  switch (cmd.msg.cmd) {
    case CmdEnum::DIRECTOR_ONLINE:
      snprintf(scratch_buffer, MAX_SCRATCH_LENGTH, "%s%d->[%d]%s guid=%d", from,
               id, cmd.msg.cmd, what, cmd.msg.reserved);
      break;

    case CmdEnum::PERFORMER_POSITION:
      snprintf(scratch_buffer, MAX_SCRATCH_LENGTH,
               "%s%d->[%d]%s ticks0=%d ticks1=%d", from, id, cmd.msg.cmd, what,
               cmd.position.ticks0, cmd.position.ticks1);
      break;

    case CmdEnum::DIRECTOR_ACCEPT:
      snprintf(scratch_buffer, MAX_SCRATCH_LENGTH, "%s%d->[%d]%s baud=%d", from,
               id, cmd.msg.cmd, what, cmd.accept.baudrate);
      break;

    case CmdEnum::DIRECTOR_PING:
    case CmdEnum::TOCK:
      snprintf(scratch_buffer, MAX_SCRATCH_LENGTH, "%s%d->[%d]%s reserved=%d",
               from, id, cmd.msg.cmd, what, cmd.msg.reserved);
      break;

    default:
      snprintf(scratch_buffer, MAX_SCRATCH_LENGTH, "%s%d->[%d]%s reserved=%d",
               from, id, cmd.msg.cmd, what, cmd.msg.reserved);
      break;
  }
  return scratch_buffer;
}

void transmit(onewire::OneCommand command) {
  ESP_LOGI(TAG, "(via onewire:) %s", format(command));
  tx.transmit(command.raw);
}

void Director::dump_config() {
  dumped = true;
  ESP_LOGI(TAG, "Master:");
  ESP_LOGI(TAG, "  pin_mode: %s", PIN_MODE);
  ESP_LOGI(TAG, "  performers: %d", _performers);
  ESP_LOGI(TAG, "  is_high_frequency: %s",
           esphome::HighFrequencyLoopRequester::is_high_frequency() ? "YES"
                                                                    : "NO!?");
  ESP_LOGI(TAG, "  message sizes (in bytes):");
  ESP_LOGI(TAG, "     Msg:      %d", sizeof(onewire::OneCommand::Msg));
  ESP_LOGI(TAG, "     Accept: %d", sizeof(onewire::OneCommand::Accept));
  tx.dump_config();
  my_channel.dump_config();
  onewire::OnewireInterrupt::dump_config();
  if (animation_controller_) animation_controller_->dump_config();
}

class WireSender {
  int value = 0;
  bool blinking = false;
  uint32_t last = 0;
  enum Mode { Blink };
  Mode mode = Blink;

 public:
  void blink() {
    auto now = millis();
    if (now - last < 2500) {
      return;
    }
    last = now;
    tx.transmit(value++);
    // blinking = !blinking;
    // ESP_LOGI(TAG, "Wire out: %s", blinking ? "HIGH" : "LOW");
    // wire.set(blinking);
    // int value = 'A';
    // ESP_LOGI(TAG, "Wire out: %d", value);
    // OneWireProtocol::write(value);
  }
} wire_controller;

class DirectorOnlineAction : public DelayAction {
 public:
  bool active = true;
  int send_count = 0;

  static constexpr int delay = 500;

  DirectorOnlineAction() : DelayAction(delay) {}

  virtual void update() override {
    if (!active) return;

    send_count++;
    if (send_count > 10000 / delay) {
      ESP_LOGW(TAG, "Message DIRECTOR_ONLINE not returned to director!?");
    }
    transmit(onewire::OneCommand::director_online(guid));
  }
} director_online_action;

class AcceptAction : public DelayAction {
 public:
  bool active = false;
  void start() { this->active = true; }

  void stop() { this->active = false; }

  virtual void update() override {
    if (!this->active) return;

    auto msg = onewire::OneCommand::Accept::create(my_channel.baudrate());
    transmit(msg);
    ESP_LOGI(TAG, "transmit: Accept(%dbaud)", msg.accept.baudrate);
  }
} accept_action;

class PingOneWireAction : public DelayAction {
  bool send = false;
  Millis t0;

 public:
  PingOneWireAction() : DelayAction(5000) {}
  virtual void update() override {
    if (!send) {
      send = true;
      t0 = millis();
      transmit(onewire::OneCommand::ping());
    } else {
      send = false;
      ESP_LOGW(TAG, "(via onewire:) Ping lost !?");
    }
  }

  void received(int _performers) {
    auto delay = millis() - t0;
    if (_performers > 0 && _performers != 24)
      ESP_LOGI(TAG,
               "(via onewire:) Performer PING duration: %dmillis (projected "
               "delay: %d)",
               delay, delay * 24 / _performers);
    else
      ESP_LOGI(TAG, "(via onewire:) Performer PING duration: %dmillis", delay);
    send = false;
  }
} ping_onewire_action;

class TickChannelAction : public DelayAction {
  bool send = false;
  Millis t0;
  uint8_t tick_id = 0;

 public:
  TickChannelAction() : DelayAction(5000) {}
  virtual void update() override {
    if (!send) {
      send = true;
      t0 = millis();
      my_channel.send(channel::messages::tick(tick_id++));
    } else {
      send = false;
      ESP_LOGW(TAG, "(via channel:) Tick lost !?");
    }
  }

  void received(int _performers) {
    auto delay = millis() - t0;
    if (_performers > 0 && _performers != 24)
      ESP_LOGI(TAG, "(via channel:) Performer TICK TOCK duration: %dmillis",
               delay);
    else
      ESP_LOGI(TAG, "(via channel:) Performer TICK TOCK duration: %dmillis",
               delay);
    send = false;
  }
} tick_action;

class TestOnewireAction : public DelayAction {
  bool send = false;
  Millis t0;

 public:
  TestOnewireAction() : DelayAction(1000) {}
  virtual void update() override {
    if (tx.transmitted()) {
      tx.transmit(3);
    }
  }
} test_onewire_action;

void Director::setup() {
  ESP_LOGI(TAG, "Master: setup!");
  this->animation_controller_ = new AnimationController();
  for (int performer_id = 0; performer_id < NMBR_OF_PERFORMERS;
       performer_id++) {
    const auto &settings = performer(performer_id);
    if (settings.animator_id >= 0)
      this->animation_controller_->remap(settings.animator_id, performer_id);
  }

  // highFrequencyLoopRequester.start();

  pinMode(LED_BUILTIN, OUTPUT);

  // what to do with the following pins ?
  // ?? const int MASTER_GPI0_PIN  = 0; // GPI00
  pinMode(MASTER_GPI0_PIN, INPUT);
  // pinMode(ESP_TXD_PIN, OUTPUT);
  // pinMode(ESP_RXD_PIN, INPUT);
  pinMode(I2C_SDA_PIN, INPUT);
  pinMode(I2C_SCL_PIN, INPUT);

  pinMode(GPIO_14, INPUT);
  pinMode(USB_POWER_PIN, INPUT);

  my_channel.setup();
  my_channel.start_transmitting();

  rx.setup();
  rx.begin();

#if MODE == MODE_ONEWIRE_VALUE || MODE == MODE_ONEWIRE_PASSTROUGH || \
    MODE == MODE_ONEWIRE_MIRROR
  tx.setup(1);
#else
  tx.setup();
#endif
  tx.begin();

#if MODE >= MODE_ONEWIRE_INTERACT
  // accept_action.start();
#endif
}

#if MODE == MODE_ONEWIRE_VALUE || MODE == MODE_ONEWIRE_PASSTROUGH || \
    MODE == MODE_ONEWIRE_MIRROR
onewire::Value value = 0;
int received = 0;

void Director::loop() {
  if (rx.pending()) {
    auto value = rx.flush();
    if (onewire::BAUD < 200)
      ESP_LOGI(TAG, "RECEIVED: {%d}", value);
    else
      ESP_LOGD(TAG, "RECEIVED: {%d}", value);
    received++;
  }
  if (tx.transmitted()) {
    delay(20);
    if (onewire::BAUD < 200)
      ESP_LOGI(TAG, "TRANSMIT: {value=%d}", value);
    else
      ESP_LOGD(TAG, "TRANSMIT: {value=%d}", value);

    tx.transmit(value++);

    if (value >= 48) {
      ESP_LOGI(TAG,
               "TRANSMIT: transmitted 48 values received %d values, uptime ",
               received);

      value = 0;
      received = 0;
    }
  }
}
#endif

#if MODE == MODE_ONEWIRE_CMD
void Director::loop() {
  if (rx.pending()) {
    auto value = rx.flush();
    if (onewire::BAUD < 200)
      ESP_LOGI(TAG, "RECEIVED: {%d}", value);
    else
      ESP_LOGD(TAG, "RECEIVED: {%d}", value);
  }
  if (tx.transmitted()) {
    delay(50);
    auto cmd = onewire::OneCommand::Accept::create(channel.baudrate());
    auto value = cmd.raw;
    if (onewire::BAUD < 200)
      ESP_LOGI(TAG, "TRANSMIT: {value=%d}", value);
    else
      ESP_LOGD(TAG, "TRANSMIT: {value=%d}", value);

    tx.transmit(value);
  }
}
#endif

#if MODE == MODE_CHANNEL
int test_count_value = 0;
void Director::loop() {
  if (!dumped) return;

#if MODE == MODE_CHANNEL
    // channel.loop();
    // tick_action.loop();

    // test_onewire_action.loop();
#endif

  Micros now = micros();
  if (rx.pending()) {
    onewire::OneCommand cmd;
    cmd.raw = rx.flush();
    if (cmd.raw == 100) {
      ESP_LOGI(TAG, "  GOT: %d", cmd.raw);
    }
  }
}
#endif

#if MODE >= MODE_ONEWIRE_INTERACT

bool pheripal_online = true;

void Director::loop() {
  if (_killed) return;

  // if (!dumped)
  //     return;

  director_online_action.loop();
  accept_action.loop();
  // accept_action.loop();
  ping_onewire_action.loop();
  tick_action.loop();
  my_channel.loop();

  if (rx.pending()) {
    onewire::OneCommand cmd;
    cmd.raw = rx.flush();
    ESP_LOGW(TAG, "Received: %s", format(cmd));
    switch (cmd.msg.cmd) {
      case CmdEnum::PERFORMER_POSITION: {
        ESP_LOGW(TAG, "PERFORMER_POSITION: performer_id = %d (%d, %d)",
                 cmd.position.source_id, cmd.position.ticks0,
                 cmd.position.ticks1);
        auto &p = performer(cmd.position.source_id);
        p.stepper0.ticks = cmd.position.ticks0;
        p.stepper1.ticks = cmd.position.ticks1;
        if (animation_controller_)
          animation_controller_->set_ticks_for_performer(
              cmd.position.source_id, cmd.position.ticks0, cmd.position.ticks1);
      } break;

      case CmdEnum::DIRECTOR_PING:
        ping_onewire_action.received(_performers);
        break;

      case CmdEnum::PERFORMER_ONLINE:
        accept_action.start();
        break;

      case CmdEnum::TOCK:
        tick_action.received(cmd.msg.source_id);
        break;

      case CmdEnum::DIRECTOR_ONLINE:
        if (!director_online_action.active) {
          ESP_LOGW(TAG,
                   "Ingore DIRECTOR_ONLINE: Ignored, most likely too slow "
                   "onewire....");
          return;
        }

        if (cmd.from_master()) {
          _performers = 0;
          ESP_LOGW(TAG,
                   "DIRECTOR_ONLINE: No performers ? Make sure, one is not "
                   "MODE_ONEWIRE_PASSTROUGH!");
        } else {
          ESP_LOGW(TAG, "DIRECTOR_ONLINE: Sending ACCEPT message");
          director_online_action.active = false;
          accept_action.start();
        }
        break;

      case CmdEnum::DIRECTOR_ACCEPT: {
        if (!accept_action.active) {
          // lets ignore, thos pne
          ESP_LOGW(TAG, "IGNORED!");
        } else if (cmd.from_master()) {
          _performers = 0;
          ESP_LOGI(TAG,
                   "DIRECTOR_ACCEPT(%d): No performers ? Make sure, one is not "
                   "MODE_ONEWIRE_PASSTROUGH!",
                   cmd.accept.baudrate);
        } else {
          accept_action.stop();

          _performers = cmd.msg.source_id + 1;
          ESP_LOGI(TAG, "DIRECTOR_ACCEPT(%d): Total performers: %d",
                   cmd.accept.baudrate, _performers);

          for (int performer_id = 0; performer_id < NMBR_OF_PERFORMERS;
               performer_id++) {
            const auto &settings = performer(performer_id);
            auto message = channel::messages::StepperSettings(
                performer_id, settings.stepper0.offset,
                settings.stepper1.offset);
            my_channel.send(message);
          }
        }
      } break;

      default:
        // ignore
        ESP_LOGW(TAG, "IGNORED: %s", format(cmd));
    }
  }
}

#endif
void Director::request_time_change(int hours, int minutes) {
  ESP_LOGW(TAG, "request_positions %d:%d", hours, minutes);
  animation::AnimationSettings settings;

  send_text(&my_channel, animation_controller_, settings,
            Text::from_time(hours, minutes));
}

void Director::request_positions() {
  auto message = channel::messages::request_positions();
  my_channel.send(message);
}

void Director::kill() {
  ESP_LOGW(TAG, "OTA detected, will kill me...");

  tx.kill();
  rx.kill();
  _killed = true;
}

#endif