
#ifdef ESP8266

const char* const TAG = "onewire-test";

#include "director_onewire_test.h"

#include "../clocks_shared/channel.h"
#include "../clocks_shared/onewire.h"
#include "../clocks_shared/onewire.interop.h"
#include "../clocks_shared/onewire.rx.h"
#include "../clocks_shared/onewire.tx.h"

using clock24::DirectorOnewireTest;

// OneWireProtocol protocol;
onewire::RxOnewire rx;
onewire::TxOnewire tx;

rs485::Gate gate;

void DirectorOnewireTest::setup() {
  gate.setup();

  ESP_LOGI(TAG, "rx.setup+begin");
  rx.setup();
  rx.begin();

  ESP_LOGI(TAG, "tx.setup+begin");
  tx.setup();
  tx.begin();

  ESP_LOGI(TAG, "Serial1.begin: %d", baudrate);
  gate.start_transmitting();
  Serial1.begin(baudrate);
}

void DirectorOnewireTest::channel_write_byte(char value) {
  LOGI(TAG, "channel_write_byte: Serial1(%dbaud)", baudrate);
  Serial1.write(value);
}

void DirectorOnewireTest::onewire_ping() {
  ESP_LOGI(TAG, "ping_onewire()");
  auto value = onewire::command_builder.ping();
  tx.transmit(value.raw);
}

void DirectorOnewireTest::onewire_accept() {
  auto msg = onewire::command_builder.accept(baudrate);
  LOGI(TAG, "transmit: Accept(%dbaud)", baudrate);
  tx.transmit(msg.raw);

  gate.start_transmitting();
  Serial1.begin(baudrate);
}

void DirectorOnewireTest::loop() {
  if (rx.pending()) {
    onewire::OneCommand cmd;
    cmd.raw = rx.flush();
    ESP_LOGI(TAG, "Received: %s", cmd.format());
  }
}

#endif