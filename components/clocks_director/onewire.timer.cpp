
#include "../clocks_shared/onewire.rx.h"
#include "../clocks_shared/onewire.tx.h"

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
  const bool state = PIN_READ(SYNC_IN_PIN);
#if MODE == MODE_ONEWIRE_PASSTROUGH
  PIN_WRITE(SYNC_OUT_PIN, state);
#ifdef DOLED
  Leds::set_ex(LED_SYNC_OUT, state ? LedColors::red : LedColors::green);
#endif
#endif
#ifdef DOLED
  auto rx = OnewireInterrupt::rx;
  if (!rx || !rx->reading())
    Leds::set_ex(LED_SYNC_IN, state ? LedColors::blue : LedColors::black);
  else
    Leds::set_ex(LED_SYNC_IN, state ? LedColors::green : LedColors::black);
#endif
}

volatile uint8_t tx_rx_cycle = 1;
volatile bool in_between_tick = false;
volatile bool use_inbetween = false;

#ifdef ESP8266
constexpr bool aligning = false;
#else
volatile bool aligning = false;
#endif

void MOVE2RAM TxTimerHandler() {
  if (!aligning) {
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

  if (OnewireInterrupt::timer_loop) OnewireInterrupt::timer_loop(micros());
}

#ifndef ESP8266
ISR(TIMER1_COMPA_vect) {
  if (aligning) return;
  TxTimerHandler();
}
#endif

void OnewireInterrupt::dump_config() {
  LOGI(TAG, "  OnewireInterrupt:");
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
  OnewireInterrupt::delay =
      float((1000000L / (BAUD * 2))) / 1000.0 / 1000.0;  // in micros

  OnewireInterrupt::timer_attach_state = ITimer1.attachInterrupt(
      1.0 / float(OnewireInterrupt::delay), TxTimerHandler);
#else
  // turn on CTC mode
  TCCR1A = 0;  // set entire TCCR1A register to 0
  TCCR1B = 0;  // same for TCCR1B
  TCCR1B |= (1 << WGM12);

  // Set CS11 bit for prescaler 1
  TCCR1B |= (1 << CS10);

  // initialize counter value to 0;
  TCNT1 = 0;

  // set timer count for 2000Hz increments
  OCR1A = 16000000L / (2 * BAUD) - 1;  // = (16*10^6) / (1*2000) - 1

  // enable timer compare interrupt
  tx_rx_cycle = 1;
  TIMSK1 |= (1 << OCIE1A);
#endif
  OnewireInterrupt::align();
}

#ifdef ESP8266
void OnewireInterrupt::align() {}
#else
void OnewireInterrupt::align() {
  aligning = true;

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

  // initialize counter value to 0;
  TCNT1 = 0;
  aligning = false;

  // submit
  transmit(onewire::OneCommand::CheckPoint::for_info('B', OCR1A / 256));
  transmit(onewire::OneCommand::CheckPoint::for_info('b', OCR1A % 256));
}
#endif

#ifdef ESP8266
void OnewireInterrupt::kill() {
  if (OnewireInterrupt::timer_attach_state >= 0) {
    ITimer1.detachInterrupt();
    OnewireInterrupt::timer_attach_state = -2;
  }
}
#endif

#ifdef ESP8266EX

void OnewireInterrupt::restart() {
  if (OnewireInterrupt::timer_attach_state < 0) return;

  ITimer1.restartTimer();
  tx_rx_cycle = 1;
}

// to be refactored
void OnewireInterrupt::disableTimer() { ITimer1.disableTimer(); }
void OnewireInterrupt::enableTimer() { ITimer1.enableTimer(); }

#endif
