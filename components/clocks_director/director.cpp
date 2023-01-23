#include "../clocks_shared/stub.h"

#if defined(IS_DIRECTOR)
#include "../clocks_shared/pinio.h"
#include "esphome/core/helpers.h"

esphome::HighFrequencyLoopRequester highFrequencyLoopRequester;

#include "../clocks_shared/async.h"
#include "../clocks_shared/async.interop.h"
#include "../clocks_shared/channel.h"
#include "../clocks_shared/channel.interop.h"
#include "../clocks_shared/onewire.rx.h"
#include "../clocks_shared/onewire.tx.h"
#include "../clocks_shared/pins.h"
#include "animation.h"
#include "director.h"

using async::Async;
using async::async_executor;
using async::async_interop;
using async::DelayAsync;
using channel::ChannelInterop;
using channel::message_builder;
using onewire::CmdEnum;
using onewire::command_builder;
using rs485::BufferChannel;
using rs485::Protocol;

uint8_t ChannelInterop::id = ChannelInterop::DIRECTOR;
int guid = rand();

bool Protocol::is_skippable_message(byte first_byte) { return true; }

static const char *const TAG = "controller";

uint8_t guid_position_ack = 0;

using clock24::Director;

Director::Director() {}

onewire::RxOnewire rx;
onewire::TxOnewire tx;

#define UART_BAUDRATE 115200
// 57600 // 256000  // 115200  // 28800  // 115200  // 9600

class DirectorChannel : public BufferChannel {
 public:
  DirectorChannel() { baudrate(UART_BAUDRATE); }

  virtual void process(const byte *bytes, const byte length) override {}
} my_channel;

void transmit(onewire::OneCommand command) {
  ESP_LOGD(TAG, "(via onewire:) %s", command.format());
  tx.transmit(command.raw);
}

void Director::dump_config() {
  dumped = true;
  LOGI(TAG, "Master:");
  LOGI(TAG, "  pin_mode: %s", PIN_MODE);
  LOGI(TAG, "  performers: %d", _performers);
  LOGI(TAG, "  is_high_frequency: %s",
       esphome::HighFrequencyLoopRequester::is_high_frequency() ? "YES"
                                                                : "NO!?");
  LOGI(TAG, "  message sizes (in bytes):");
  LOGI(TAG, "     Msg:        %d", sizeof(onewire::OneCommand::Msg));
  LOGI(TAG, "     Accept:     %d", sizeof(onewire::OneCommand::Accept));
  LOGI(TAG, "     CheckPoint: %d", sizeof(onewire::OneCommand::CheckPoint));
  // LOGI(TAG, "  SERIAL_RX_BUFFER_SIZE: %d", SERIAL_RX_BUFFER_SIZE);
  // LOGI(TAG, "  SERIAL_TX_BUFFER_SIZE: %d", SERIAL_TX_BUFFER_SIZE);

  if (wi_fi_component == nullptr) {
    LOGI(TAG, "  wifi: nullpntr");
  } else {
    LOGI(TAG, "  wifi: %s",
         wi_fi_component->is_connected() ? "CONNECTED" : "NOT connected");
  }
  tx.dump_config();
  my_channel.dump_config();
  onewire::OnewireInterrupt::dump_config();
  if (this->animation_controller_) this->animation_controller_->dump_config();
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
    // LOGI(TAG, "Wire out: %s", blinking ? "HIGH" : "LOW");
    // wire.set(blinking);
    // int value = 'A';
    // LOGI(TAG, "Wire out: %d", value);
    // OneWireProtocol::write(value);
  }
} wire_controller;

class DirectorOnlineAction : public IntervalAction {
 public:
  bool active = true;
  int send_count = 0;

  static constexpr int delay = 500;

  DirectorOnlineAction() : IntervalAction(delay) {}

  virtual void update() override {
    if (!active) return;

    send_count++;
    if (send_count > 10000 / delay) {
      ESP_LOGW(TAG, "Message DIRECTOR_ONLINE not returned to director!?");
    }
    transmit(command_builder.director_online(guid));
  }
} director_online_action;

class AcceptAction : public IntervalAction {
 public:
  bool active = false;
  void start() { this->active = true; }

  void stop() { this->active = false; }

  virtual void update() override {
    if (!this->active) return;

    auto msg = command_builder.accept(my_channel.baudrate());
    transmit(msg);
    LOGI(TAG, "transmit: Accept(%dbaud)", msg.accept.baudrate);
  }
} accept_action;

class PingOneWireAction : public IntervalAction {
  bool send = false;
  Millis t0;

 public:
  PingOneWireAction() : IntervalAction(500) {}
  virtual void update() override {
    if (!send) {
      send = true;
      t0 = millis();
      transmit(command_builder.ping());
    } else {
      send = false;
      ESP_LOGW(TAG, "(via onewire:) Ping lost !?");
    }
  }

  void received(int _performers) {
    auto delay = millis() - t0;
    if (_performers > 0)
      ESP_LOGD(TAG,
               "(via onewire:) Performer PING duration: %dmillis (projected "
               "delay: %d)",
               delay, delay * 24 / _performers);
    else
      LOGI(TAG, "(via onewire:) Performer PING duration: %dmillis", delay);
    send = false;
  }
} ping_onewire_action;

class TickChannelAction : public IntervalAction {
  bool send = false;
  Millis t0;
  uint8_t tick_id = 0;

 public:
  TickChannelAction() : IntervalAction(5000) {}
  virtual void update() override {
    if (!send) {
      send = true;
      t0 = millis();
      my_channel.send(message_builder.tick(tick_id++));
    } else {
      send = false;
      ESP_LOGW(TAG, "(via channel:) Tick lost !?");
    }
  }

  void received(int performer_id) {
    auto delay = millis() - t0;
    ESP_LOGD(TAG, "(via channel:) Performer%d  TICK TOCK duration: %dmillis",
             performer_id, delay);
    send = false;
  }
} tick_action;

class TestOnewireAction : public IntervalAction {
  bool send = false;
  Millis t0;

 public:
  TestOnewireAction() : IntervalAction(1000) {}
  virtual void update() override {
    if (tx.transmitted()) {
      tx.transmit(3);
    }
  }
} test_onewire_action;

void Director::setup() {}

bool Director::is_started() {
  if (_killed) return true;
  if (!async::interop::suspended) return true;
  if (!this->wi_fi_component->is_connected()) return false;

  start();
  async::interop::suspended = false;
  return true;
}

void Director::start() {
  async_interop.set_channel(this->get_channel());
  async_interop.set_tx_onewire(&tx);

  LOGI(TAG, "Master: setup!");
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

  // Serial.setRxBufferSize(256);
  // Serial.setTxBufferSize(1024);

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
  async::interop::suspended = false;
}

#if MODE == MODE_ONEWIRE_VALUE || MODE == MODE_ONEWIRE_PASSTROUGH || \
    MODE == MODE_ONEWIRE_MIRROR
onewire::Value value = 0;
int received = 0;

void Director::loop() {
  if (!is_started()) return;

  while (rx.pending()) {
    auto value = rx.flush();
    if (onewire::USED_BAUD < 2000)
      LOGI(TAG, "RECEIVED: {%d}", value);
    else
      ESP_LOGD(TAG, "RECEIVED: {%d}", value);
    received++;
  }
  if (tx.transmitted()) {
    delay(20);
    if (onewire::USED_BAUD < 2000)
      LOGI(TAG, "TRANSMIT: {value=%d}", value);
    else
      ESP_LOGD(TAG, "TRANSMIT: {value=%d}", value);

    tx.transmit(value++);

    if (value >= 48) {
      LOGI(TAG, "TRANSMIT: transmitted 48 values received %d values, uptime ",
           received);

      value = 0;
      received = 0;
    }
  }
}
#endif

#if MODE == MODE_ONEWIRE_CMD
int baud_idx = 0;
void Director::loop() {
  if (!is_started()) return;

  while (rx.pending()) {
    auto value = rx.flush();
    if (onewire::USED_BAUD < 200)
      LOGI(TAG, "RECEIVED: {%d}", value);
    else
      LOGI(TAG, "RECEIVED: {%d}", value);
  }
  if (tx.transmitted()) {
    auto cmd = command_builder.accept(baud_idx++);
    auto value = cmd.raw;
    if (onewire::USED_BAUD < 200)
      LOGI(TAG, "TRANSMIT: {value=%d}", value);
    else
      LOGI(TAG, "TRANSMIT: {value=%d}", value);

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
      LOGI(TAG, "  GOT: %d", cmd.raw);
    }
  }
}
#endif

#if MODE >= MODE_ONEWIRE_INTERACT

bool pheripal_online = true;

void Director::loop() {
  if (_killed) return;
  if (!is_started()) return;

  // if (!dumped)
  //     return;

  director_online_action.loop();
  accept_action.loop();
  // accept_action.loop();
  ping_onewire_action.loop();
  tick_action.loop();
  my_channel.loop();
  if (accepted) async_executor.loop();

  if (rx.pending()) {
    onewire::OneCommand cmd;
    cmd.raw = rx.flush();
    ESP_LOGD(TAG, "Received: %s", cmd.format());
    switch (cmd.msg.cmd) {
      case CmdEnum::PERFORMER_CHECK_POINT:
        if (cmd.check_point.debug)
          ESP_LOGD(TAG, "%s", cmd.format());
        else
          LOGI(TAG, "%s", cmd.format());
        if (cmd.msg.source_id >= 0 && cmd.msg.source_id < 24) {
          auto &performer = performers[cmd.msg.source_id];
          if (cmd.check_point.id == 'C')
            performer.channel_errors = cmd.check_point.value;
          else if (cmd.check_point.id == 'O')
            performer.one_wire_errors = cmd.check_point.value;
          else if (cmd.check_point.id == 'S')
            performer.channel_skips = cmd.check_point.value;
          else if (cmd.check_point.id == 'U')
            performer.uptime_in_seconds = cmd.check_point.value;
        }
        break;

      case CmdEnum::PERFORMER_POSITION: {
        ESP_LOGW(TAG, "PERFORMER_POSITION: performer_id = %d (%f, %f)",
                 cmd.position.source_id, cmd.position.ticks0 / 720.0 * 12.0,
                 cmd.position.ticks1 / 720.0 * 12.0);
        auto &p = performer(cmd.position.source_id);
        p.stepper0.ticks = cmd.position.ticks0;
        p.stepper1.ticks = cmd.position.ticks1;
        if (animation_controller_)
          animation_controller_->set_ticks_for_performer(
              cmd.position.source_id, cmd.position.ticks0, cmd.position.ticks1);
      } break;

      case CmdEnum::DIRECTOR_POSITION_ACK:
        ESP_LOGW(TAG, "DIRECTOR_POSITION_ACK: puid=%d", cmd.position_ack.puid);
        guid_position_ack++;
        break;

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
          LOGI(TAG,
               "DIRECTOR_ACCEPT(%d): No performers ? Make sure, one is not "
               "MODE_ONEWIRE_PASSTROUGH!",
               cmd.accept.baudrate);
        } else {
          ping_onewire_action.set_delay_in_millis(5000);

          accept_action.stop();

          _performers = cmd.msg.source_id + 1;
          LOGI(TAG, "DIRECTOR_ACCEPT(%d): Total performers: %d",
               cmd.accept.baudrate, _performers);

          for (int performer_id = 0; performer_id < NMBR_OF_PERFORMERS;
               performer_id++) {
            const auto &settings = performer(performer_id);
            auto message = channel::messages::PerformerSettings(
                performer_id, settings.stepper0.offset,
                settings.stepper1.offset);
            my_channel.send(message);

            this->accepted = true;
          }
          on_attach();
        }
      } break;

      default:
        // ignore
        ESP_LOGW(TAG, "IGNORED: %s", cmd.format());
    }
  }
}

#endif

class RequestPositionsAsync : public DelayAsync {
  const uint8_t current_position_ack;
  Director *director;

 public:
  RequestPositionsAsync(Director *director)
      : DelayAsync(1500),
        current_position_ack(guid_position_ack),
        director(director) {}

  void request() {
    LOGI(TAG, "RequestPositionsAsync: REQUEST");

    my_channel.send(message_builder.request_kill_keys_or_request_position(
        current_position_ack));
    // my_channel.send(message_builder.request_positions());
    transmit(command_builder.director_position_ack(current_position_ack));
  }

  Async *first() override {
    LOGI(TAG, "RequestPositionsAsync: FIRST");
    request();
    return this;
  }

  Async *update() override {
    LOGI(TAG, "RequestPositionsAsync: UPDATE");
    if (age_in_millis() > 10000) {
      LOGI(TAG, "Took too long to obtain positions... killing async.");
      return nullptr;
    } else {
      request();
      return this;
    }
  }

  Async *loop() override {
    if (guid_position_ack == current_position_ack) {
      return DelayAsync::loop();
    }
    return nullptr;
  }
};

void Director::request_positions() {
  async_executor.queue(new RequestPositionsAsync(this));
}

void Director::kill() {
  ESP_LOGW(TAG, "OTA detected, will kill me...");

  tx.kill();
  rx.kill();
  _killed = true;
  async::interop::suspended = true;
}

BufferChannel *Director::get_channel() { return &my_channel; }

#endif