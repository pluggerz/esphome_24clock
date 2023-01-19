#include "onewire.interop.h"

#ifdef ESP8266
char onewire::scratch_buffer[onewire::MAX_SCRATCH_LENGTH];
#endif

// keep empty, just testing if the header can live without any other

// a hack should be cleaned up

#include "../clocks_shared/log.h"
#include "../clocks_shared/onewire.rx.h"

void onewire::RxOnewire::debug(onewire::Value value) {
  LOGD(__FILE__, "Got: %s", ((onewire::OneCommand*)&value)->format());
}
