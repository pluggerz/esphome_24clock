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
      LOGE(TAG, "Unable to queue, because tx not set!");
      return nullptr;
    }
    if (!tx_onewire->transmitted()) {
      return this;
    }
    tx_onewire->transmit(command);
    return nullptr;
  }
};

void AsyncInterop::queue_async(Async *action) { async_executor.queue(action); }

void AsyncInterop::queue_command(const Value &command) {
  /*if (tx_onewire == nullptr) {
    LOGE(TAG, "Unable to queue, because tx not set!");
    return;
  }
  tx_onewire->transmit(command);*/
  queue_async(new CommandAsyncRequest(this->get_tx_onewire(), command));
}

void AsyncInterop::queue_raw_message(const byte *bytes, int length) {
  if (channel == nullptr) {
    LOGE(TAG, "Unable to queue, because tx not set!");
    return;
  }
  channel->raw_send(bytes, length);
}

#endif