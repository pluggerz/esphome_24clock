
#include "../clocks_shared/onewire.interop.h"
#include "../clocks_shared/onewire.rx.h"
#include "../clocks_shared/onewire.tx.h"

using onewire::command_builder;
using onewire::OnewireInterrupt;
using onewire::RxOnewire;
using onewire::TimerLoop;
using onewire::TxOnewire;

TxOnewire *OnewireInterrupt::tx = nullptr;
RxOnewire *OnewireInterrupt::rx = nullptr;
TimerLoop OnewireInterrupt::timer_loop = nullptr;

float OnewireInterrupt::delay = -1;

int OnewireInterrupt::timer_attach_state = -2;

#ifdef ESP8266
//  Credits go: https://github.com/khoih-prog/ESP8266TimerInterrupt

// Select a Timer Clock
#define USING_TIM_DIV1 true     // for shortest and most accurate timer
#define USING_TIM_DIV16 false   // for medium time and medium accurate timer
#define USING_TIM_DIV256 false  // for longest timer but least accurate. Default

#include <ESP8266_ISR_Timer.h>

#include "ESP8266TimerInterrupt.h"
#include "ESP8266_ISR_Timer.hpp"

// Init ESP8266 only and only Timer 1
ESP8266Timer ITimer1;

const char *const TAG = "1wireTimer";

#define DOLOG

#else

#endif

MOVE2RAM void follow_change() {
  auto rx = OnewireInterrupt::rx;
  if (rx) rx->follow_change();
#if MODE == MODE_ONEWIRE_PASSTROUGH
  const bool led_state = PIN_READ(SYNC_IN_PIN);
  PIN_WRITE(SYNC_OUT_PIN, led_state);
#ifdef DOLED
  Leds::set_ex(LED_SYNC_OUT, led_state ? LedColors::red : LedColors::green);
#endif
#endif
#ifdef DOLED
  const bool state = PIN_READ(SYNC_IN_PIN);
  if (!rx || !rx->reading())
    Leds::set_ex(LED_SYNC_IN, state ? LedColors::blue : LedColors::black);
  else
    Leds::set_ex(LED_SYNC_IN, state ? LedColors::green : LedColors::black);
#endif
}

void MOVE2RAM TxTimerHandler() {
  auto tx = OnewireInterrupt::tx;
  if (tx && tx->active()) tx->timer_interrupt();
}

#ifndef ESP8266

class FrequencyChecker {
 public:
  Micros start = 0L;
  const int wait_in_seconds = 60;
  uint32_t ticks = 0;

  const char ch;

  FrequencyChecker(char ch) : ch(ch) {}
  void loop() {
    auto now = millis();
    if (start == 0L) {
      ticks = 0;
      start = now;
      return;
    }
    if (now - start < 1000 * wait_in_seconds) {
      ticks++;
      return;
    }
    transmit(onewire::OneCommand::CheckPoint::for_debug(
        ch, ticks / wait_in_seconds));
    ticks = 0;
    start = now;
  }
} frequencyChecker1('1'), frequencyChecker2('2');

template <int PIN>
class LedTracker {
 public:
  uint16_t counter = 0;
  uint32_t data_hits = 0;
  bool first_color = true;
  const uint16_t hit0;
  const uint16_t hit1;
  LedTracker(uint16_t hit) : hit0(hit), hit1(2 * hit) {}
  void loop() {
    if (this->counter == 0 || this->counter == this->hit0) {
      first_color = !first_color;
      this->counter++;
      this->data_hits = 0;
    } else if (counter == this->hit1) {
      first_color = !first_color;
      this->counter = 0;
      this->data_hits = 0;
    } else
      this->counter++;

#ifdef DOLED
    Leds::set_ex(PIN,
                 rgb_color(first_color ? 0xFF : 0x00, first_color ? 0x00 : 0xff,
                           data_hits ? 0xFF : 0x00));
#endif
  }
};

// communication is around 1000 BAUD, the clock on 2000 BAUD
LedTracker<LED_TIMER_ONEWIRE> onewire_timer_tracker(1600);
// communication is around 4000Hz
LedTracker<LED_TIMER_STEPPER> onewire_steppers_tracker(1600);

ISR(TIMER1_COMPA_vect) {
  onewire_steppers_tracker.loop();
  frequencyChecker1.loop();
  if (OnewireInterrupt::timer_loop) OnewireInterrupt::timer_loop(micros());
#ifdef TIMER1_FALLBACK
  onewire_timer_tracker.loop();
  TxTimerHandler();
  auto rx = OnewireInterrupt::rx;
  if (rx && rx->reading()) {
    onewire_timer_tracker.data_hits++;
  }
#endif
}

ISR(TIMER2_COMPA_vect) {
  frequencyChecker2.loop();
  onewire_timer_tracker.loop();

#ifndef TIMER1_FALLBACK
  TxTimerHandler();
  auto rx = OnewireInterrupt::rx;
  if (rx && rx->reading()) {
    onewire_timer_tracker.data_hits++;
  }
#endif
}
#endif

void OnewireInterrupt::dump_config() {
  LOGI(TAG, "  OnewireInterrupt:");
  LOGI(TAG, "     speed: %fbaud", (float)onewire::USED_BAUD);
  LOGI(TAG, "     delay: %fsec", OnewireInterrupt::delay);
  LOGI(TAG, "     delay: %fmillis", OnewireInterrupt::delay * 1000.0);
  LOGI(TAG, "     delay: %fmicros", OnewireInterrupt::delay * 1000.0 * 1000.0);
}

void attach_sync_in_interupt() {
  auto interupt = digitalPinToInterrupt(SYNC_IN_PIN);
  if (interupt < 0) {
#ifdef DOLED
    Leds::error(LEDS_ERROR_NO_INTERRUPT);
#endif
    ESP_LOGW(TAG, "Unable to attach interrupt");
  }
  attachInterrupt(interupt, follow_change, CHANGE);
}

void OnewireInterrupt::attach() {
  if (OnewireInterrupt::timer_attach_state >= 0) return;

  attach_sync_in_interupt();

#ifdef ESP8266
  // note we use attachInterrupt, instead of attachInterruptInterval since ESP
  // library assumes micros, while arduino assumes millis
  OnewireInterrupt::delay = float((double(1000000.0) / double(USED_BAUD))) /
                            double(1000.0) / double(1000.0);  // in micros

  OnewireInterrupt::timer_attach_state = ITimer1.attachInterrupt(
      1.0 / float(OnewireInterrupt::delay), TxTimerHandler);
#else
  // from: https://www.instructables.com/Arduino-Timer-Interrupts/
  // or: https://playground2014.wordpress.com/arduino/basics-timer-interrupts/
  // TIMER1
  // turn on CTC mode
  TCCR1A = 0;  // set entire TCCR1A register to 0
  TCCR1B = 0;  // same for TCCR1B
  TCCR1B |= (1 << WGM12);
  // Set CS11 bit for prescaler 8
  TCCR1B |= (1 << CS11);
  // initialize counter value to 0;
  TCNT1 = 0;
  // set timer count for 2000Hz increments
  // = (16*10^6) / (1*2000) - 1
#ifdef TIMER1_FALLBACK
#define FREQ USED_BAUD
#else
#define FREQ 4000
#endif
  OCR1A = double(16000000L) / double(FREQ) / 8 - 1;

  // TIMER2
  // double check:
  // https://microcontrollerslab.com/arduino-timer-interrupts-tutorial/ turn on
  // CTC mode
  TCCR2A = 0;  // set entire TCCR2A register to 0
  TCCR2B = 0;  // same for TCCR2B
  TCCR2A |= (1 << WGM21);
  // Set all CS2x bits for prescaler 1024
  // TCCR2B |= (1 << CS20);

  // 64 ?
  TCCR2B = (0 << CS22) | (0 << CS21) | (1 << CS20);

  // initialize counter value to 0;
  TCNT2 = 0;
  // set timer count for 2000Hz increments
  // if (UNO_TIMER2_OCR2A >= 255) {
  //  Leds::error(LEDS_ERROR_TIMER2_FAILED);
  //}
  OCR2A = determine_ocr2a(TIMER2_PRESCALER);
  if (TIMER2_PRESCALER == 1024) {
    // officially
    TCCR2B = (1 << CS22) | (0 << CS21) | (1 << CS20);
    // but this one works ?!?
    TCCR2B = (1 << CS22) | (1 << CS21) | (1 << CS20);
  } else if (TIMER2_PRESCALER == 256) {
    // officially
    TCCR2B = (1 << CS22) | (0 << CS21) | (0 << CS20);
    // but this one works ?!?
    TCCR2B = (1 << CS22) | (1 << CS21) | (0 << CS20);
  } else if (TIMER2_PRESCALER == 128) {
    // but this one works ?!? 128 ??
    TCCR2B = (0 << CS22) | (1 << CS21) | (1 << CS20);
  } else if (TIMER2_PRESCALER == 64) {
    // officially
    TCCR2B = (0 << CS22) | (1 << CS21) | (1 << CS20);
    // but this one works ?!? 128 ?
    TCCR2B = (0 << CS22) | (1 << CS21) | (1 << CS20);
  } else {
    Leds::error(6);
  }
  // enable timers compare interrupt
  TIMSK1 |= (1 << OCIE1A);
  TIMSK2 |= (1 << OCIE2A);
#endif
}

#ifdef ESP8266
void OnewireInterrupt::kill() {
  if (OnewireInterrupt::timer_attach_state >= 0) {
    ITimer1.detachInterrupt();
    OnewireInterrupt::timer_attach_state = -2;
  }
}
#endif

#ifdef ESP8266EX

// to be refactored
void OnewireInterrupt::disableTimer() { ITimer1.disableTimer(); }
void OnewireInterrupt::enableTimer() { ITimer1.enableTimer(); }

#endif
