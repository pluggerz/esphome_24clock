#pragma once

#ifdef ESP8266
#include <deque>

#include "../clocks_shared/onewire.interop.h"
#include "../clocks_shared/shared.types.h"

namespace async {
const char* const TAG = "async";

class Async {
 public:
  /***
   * return null if done, itself to continue or another one to be put in place
   * (itself will be deleted!)
   *
   */
  virtual Async* loop() = 0;

  virtual ~Async() {}
};

class DelayAsync : public Async {
  Millis t0 = 0L;
  Millis t1 = 0L;
  const Millis delay_in_millis;

 protected:
  virtual Async* loop();
  Millis age_in_millis() const;
  virtual Async* first() { return update(); }
  virtual Async* update() = 0;

 public:
  DelayAsync(Millis delay_in_millis) : delay_in_millis(delay_in_millis) {}
};

class AsyncExecutor {
  std::deque<Async*> deque;

 public:
  void queue(Async* async);
  void loop();
} extern async_executor;
}  // namespace async
#endif