#pragma once

#ifdef ESP8266
#include <deque>

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
  virtual Async* loop() {
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

  Millis age_in_millis() const {
    return this->t0 == 0L ? 0 : millis() - this->t0;
  }
  virtual Async* first() = 0;
  virtual Async* update() = 0;

 public:
  DelayAsync(Millis delay_in_millis) : delay_in_millis(delay_in_millis) {}
};

class AsyncExecutor {
  static std::deque<Async*> deque;

 public:
  static void queue(Async* async) {
    LOGI(TAG, "Push to back: %d", async);
    deque.push_back(async);
  }
  static void loop() {
    if (deque.empty()) return;

    Async* front = deque.front();
    Async* next = front->loop();
    if (front == next) {
      return;
    }

    LOGI(TAG, "Deleting async: %d", front);
    delete front;
    if (next) {
      LOGI(TAG, "Replace front with %d", next);
      deque[0] = next;
    } else {
      LOGI(TAG, "Popping %d from front", front);
      next = deque.front();
      deque.pop_front();
      if (next != front) {
        LOGI(TAG, "Popped value %d is not equal to front %d", next, front);
      }
    }
  }
};
}  // namespace async
#endif