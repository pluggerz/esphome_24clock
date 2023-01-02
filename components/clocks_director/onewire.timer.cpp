#include "../clocks_shared/onewire.rx.h"
#include "../clocks_shared/onewire.tx.h"

using onewire::OnewireInterrupt;
using onewire::RxOnewire;
using onewire::TxOnewire;

TxOnewire *OnewireInterrupt::tx = nullptr;
RxOnewire *OnewireInterrupt::rx = nullptr;

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
// Select a Timer Clock
// Select the timers you're using, here ITimer1
#define USE_TIMER_1 true
#define USE_TIMER_2 false
#define USE_TIMER_3 false
#define USE_TIMER_4 false
#define USE_TIMER_5 false

//  Credits go: https://github.com/khoih-prog/TimerInterrupt

#include "TimerInterrupt.h"
#endif

MOVE2RAM void follow_change() {
  const bool state = PIN_READ(SYNC_IN_PIN);
#if MODE == MODE_ONEWIRE_PASSTROUGH
  PIN_WRITE(SYNC_OUT_PIN, state);
#ifdef DOLED
  Leds::set_ex(LED_SYNC_OUT, state ? LedColors::red : LedColors::green);
#endif
#endif
#ifdef DOLED
  auto rx = onewire::OnewireInterrupt::rx;
  if (!rx || !rx->reading())
    Leds::set_ex(LED_SYNC_IN, state ? LedColors::blue : LedColors::black);
  else
    Leds::set_ex(LED_SYNC_IN, state ? LedColors::green : LedColors::black);
#endif
}

volatile uint8_t tx_rx_cycle = 1;
volatile bool in_between_tick = false;
volatile bool use_inbetween = false;

void MOVE2RAM TxTimerHandler() {
  if (tx_rx_cycle & 1) {
    auto tx = OnewireInterrupt::tx;
    if (!tx || !tx->active()) {
      // ignore
    } else if (in_between_tick) {
      PIN_WRITE(SYNC_OUT_PIN, false);
      in_between_tick = false;
#ifdef DOLED
      Leds::set_ex(LED_SYNC_OUT, LedColors::black);
#endif
    } else if (tx->transmitted()) {
      if (use_inbetween) {
        in_between_tick = !in_between_tick;
        if (in_between_tick) PIN_WRITE(SYNC_OUT_PIN, true);
#ifdef DOLED
        Leds::set_ex(LED_SYNC_OUT,
                     in_between_tick ? LedColors::blue : LedColors::black);
#endif
      }
    } else {
      tx->timer_interrupt();
      in_between_tick = false;
    }
  } else {
    auto interrupt = OnewireInterrupt::rx;
    if (interrupt != nullptr) interrupt->timer_interrupt();
  }
  tx_rx_cycle++;
}

void OnewireInterrupt::dump_config() {
  ESP_LOGI(TAG, "  OnewireInterrupt:");
  ESP_LOGI(TAG, "     no info");
}

void OnewireInterrupt::kill() {
  if (OnewireInterrupt::timer_attach_state >= 0) {
    ITimer1.detachInterrupt();
    OnewireInterrupt::timer_attach_state = -2;
  }
}

void OnewireInterrupt::restart() {
  if (OnewireInterrupt::timer_attach_state < 0) return;

  ITimer1.restartTimer();
  tx_rx_cycle = 1;
}

#ifndef ESP8266
void OnewireInterrupt::align() {
  Leds::set_ex(LED_MODE, LedColors::purple);

  // wait for first change
  Leds::blink(LedColors::purple, 1);
  for (int idx = 2; idx < 4; ++idx) {
    bool last_state = false;
    while (true) {
      bool state = PIN_READ(SYNC_IN_PIN);
      if (state == false && last_state == true) break;
      last_state = state;
    }
  }
  ITimer1.restartTimer();
  Leds::blink(LedColors::black, 1);
}
#else
void OnewireInterrupt::align() {}
#endif

void OnewireInterrupt::attach() {
  if (OnewireInterrupt::timer_attach_state >= 0) return;
#ifndef ESP8266
  ITimer1.init();
#endif

  // note we use attachInterrupt, instead of attachInterruptInterval since ESP
  // library assumes micros, while arduino assumes millis
  float delay = float((1000000L / (BAUD * 2))) / 1000.0 / 1000.0;  // in micros

  auto interupt = digitalPinToInterrupt(SYNC_IN_PIN);
  if (interupt < 0) {
#ifdef DOLED
    Leds::error(LEDS_ERROR_NO_INTERRUPT);
#endif
    ESP_LOGW(TAG, "Unable to attach interrupt");
  }
  attachInterrupt(interupt, follow_change, CHANGE);

  OnewireInterrupt::timer_attach_state =
      ITimer1.attachInterrupt(1.0 / float(delay), TxTimerHandler);
  OnewireInterrupt::align();
}

// to be refactored
void OnewireInterrupt::disableTimer() { ITimer1.disableTimer(); }
void OnewireInterrupt::enableTimer() { ITimer1.enableTimer(); }
