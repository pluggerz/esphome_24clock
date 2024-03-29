#pragma once

#include "async.h"
#include "channel.h"
#include "channel.interop.h"
#include "onewire.h"

namespace async {

namespace interop {
const char *const TAG = "async.interop";

using onewire::OneCommand;
using onewire::TxOnewire;

extern bool suspended;

using rs485::BufferChannel;
class AsyncInterop {
 protected:
  BufferChannel *channel = nullptr;
  TxOnewire *tx_onewire = nullptr;

 public:
  BufferChannel *get_channel() const { return this->channel; }
  void set_channel(BufferChannel *channel) { this->channel = channel; }

  TxOnewire *get_tx_onewire() { return this->tx_onewire; }
  void set_tx_onewire(TxOnewire *tx_onewire) { this->tx_onewire = tx_onewire; }

  void queue_async(Async *action);
  void queue_command(const OneCommand &command);

  void queue_raw_message(const byte *m, int size);

  template <class M>
  void queue_message(const M &m) {
    queue_raw_message((const byte *)&m, sizeof(M));
  }

  void direct_raw_message(const byte *m, int size);

  bool direct_raw_message_bytes_available_for_write(int bytes);

  template <class M>
  void direct_message(const M &m) {
    direct_raw_message((const byte *)&m, sizeof(M));
  }
};
}  // namespace interop
extern interop::AsyncInterop async_interop;

}  // namespace async