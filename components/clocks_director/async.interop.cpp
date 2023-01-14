#ifdef ESP8266

#include "../clocks_shared/async.interop.h"

#include "../clocks_shared/log.h"
#include "../clocks_shared/onewire.tx.h"

using namespace async::interop;

using async::Async;
using async::async_executor;
using async::interop::AsyncInterop;
using onewire::TxOnewire;
using onewire::Value;

class CommandAsyncRequest : public Async {
  const Value command;
  TxOnewire *tx_onewire;

 public:
  CommandAsyncRequest(TxOnewire *tx_onewire, const Value &command)
      : tx_onewire(tx_onewire), command(command) {}

  Async *loop() override {
    if (tx_onewire == nullptr) {
      LOGE(TAG, "Unable to queue, because tx is not set!");
      return nullptr;
    }
    if (!tx_onewire->transmitted()) {
      return this;
    }
    LOGE(TAG, "Command transmitted!");
    tx_onewire->transmit(command);
    return nullptr;
  }
};

class MessageAsyncRequest : public Async {
  BufferChannel *channel;
  int size;
  byte *bytes;

 public:
  MessageAsyncRequest(BufferChannel *channel, const byte *msg_bytes, int size)
      : channel(channel), size(size) {
    this->bytes = new byte[size];
    memcpy(this->bytes, msg_bytes, size);
  }

  ~MessageAsyncRequest() { delete[] this->bytes; }

  Async *loop() override {
    if (channel == nullptr) {
      LOGE(TAG, "Unable to queue, because channel is not set!");
      return nullptr;
    }
    if (!channel->ready()) {
      return this;
    }
    LOGD(TAG, "Message transmitted!");
    channel->raw_send(bytes, size);
    return nullptr;
  }
};

void AsyncInterop::queue_async(Async *action) { async_executor.queue(action); }

void AsyncInterop::queue_command(const Value &command) {
  queue_async(new CommandAsyncRequest(this->get_tx_onewire(), command));
}

void AsyncInterop::queue_raw_message(const byte *bytes, int length) {
  if (channel == nullptr) {
    LOGE(TAG, "Unable to queue, because tx not set!");
    return;
  }
  // channel->raw_send(bytes, length);
  queue_async(new MessageAsyncRequest(this->get_channel(), bytes, length));
}

void AsyncInterop::direct_raw_message(const byte *bytes, int length) {
  if (channel == nullptr) {
    LOGE(TAG, "Unable to queue, because tx not set!");
    return;
  }
  channel->raw_send(bytes, length);
}

#endif