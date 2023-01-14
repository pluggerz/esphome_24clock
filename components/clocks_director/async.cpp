#ifdef ESP8266

#include "../clocks_shared/async.h"

#include "../clocks_shared/log.h"
#include "Arduino.h"

using namespace async;

//// DelayAsync

Async* DelayAsync::loop() {
  if (this->t0 == 0L) {
    this->t0 = this->t1 = millis();
    return first();
  }
  Millis now = millis();
  if (now - this->t1 < this->delay_in_millis) {
    return this;
  }
  this->t1 = now;
  return update();
};

Millis DelayAsync::age_in_millis() const {
  return this->t0 == 0L ? 0 : millis() - this->t0;
}

//// AsyncExecutor

void AsyncExecutor::queue(Async* async) {
  LOGV(TAG, "Push to back: %d", async);
  deque.push_back(async);
}
void AsyncExecutor::loop() {
  if (deque.empty()) return;

  Async* front = deque.front();
  Async* next = front->loop();
  if (front == next) {
    return;
  }

  LOGV(TAG, "Deleting async: %d", front);
  delete front;
  if (next) {
    LOGV(TAG, "Replace front with %d", next);
    deque[0] = next;
  } else {
    LOGV(TAG, "Popping %d from front", front);
    next = deque.front();
    deque.pop_front();
    if (next != front) {
      LOGE(TAG, "Popped value %d is not equal to front %d", next, front);
    }
  }
}
#endif