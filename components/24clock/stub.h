#pragma once

#ifdef ESP8266
#define MASTER

#include "stub.master.h"

#else
#define SLAVE

#include "slave/stub.slave.h"

#endif