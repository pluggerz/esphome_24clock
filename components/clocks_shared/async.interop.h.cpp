#include "async.interop.h"

using async::interop::AsyncInterop;

AsyncInterop async::async_interop;
bool async::interop::suspended = true;

// keep empty, just testing if the header can live without any other