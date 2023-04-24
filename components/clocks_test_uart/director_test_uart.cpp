
#ifdef ESP8266

const char* const TAG = "onewire-test";

#include "director_test_uart.h"

#include "../clocks_shared/channel.h"
#include "../clocks_shared/channel.interop.h"
#include "../clocks_shared/onewire.h"
#include "../clocks_shared/onewire.interop.h"
#include "../clocks_shared/onewire.rx.h"
#include "../clocks_shared/onewire.tx.h"

using channel::message_builder;
using clock24::DirectorTestUart;
using rs485::BufferChannel;

// OneWireProtocol protocol;
onewire::RxOnewire rx;
onewire::TxOnewire tx;

rs485::Gate gate;

class DirectorChannel : public BufferChannel {
 public:
  virtual void process(const byte* bytes, const byte length) override {}
} my_channel;

void DirectorTestUart::setup() {
  gate.setup();

  ESP_LOGI(TAG, "rx.setup+begin");
  rx.setup();
  rx.begin();

  ESP_LOGI(TAG, "tx.setup+begin");
  tx.setup();
  tx.begin();

  ESP_LOGI(TAG, "Serial1.begin: %d", baudrate);
  my_channel.setup();
  my_channel.begin(UART_BAUDRATE);
  my_channel.start_transmitting();
}

void DirectorTestUart::onewire_init() {
  Serial1.begin(baudrate);
  gate.start_transmitting();

  auto msg = onewire::command_builder.accept(baudrate);
  LOGI(TAG, "transmit: Accept(%dbaud)", baudrate);
  tx.transmit(msg.raw);
}

void DirectorTestUart::channel_write_byte(char value) {
  LOGI(TAG, "channel_write_byte: Serial1(%dbaud)", baudrate);
  Serial1.write(value);
}

int tick_id = 0;

void DirectorTestUart::channel_test_message() {
  LOGI(TAG, "channel_test_message: Serial1(%dbaud)", baudrate);
  auto msg = message_builder.tick(0x12);
  my_channel.send(msg);
}

void DirectorTestUart::loop() {
  if (rx.pending()) {
    onewire::OneCommand cmd;
    cmd.raw = rx.flush();
    ESP_LOGI(TAG, "Received: %s", cmd.format());
  }
}

#endif