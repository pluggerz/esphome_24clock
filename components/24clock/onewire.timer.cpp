#include "onewire.h"

#ifdef TX_TIMER

using onewire::OnewireInterrupt;

OnewireInterrupt *OnewireInterrupt::rx = nullptr;
OnewireInterrupt *OnewireInterrupt::tx = nullptr;

int OnewireInterrupt::timer_attach_state = -2;

#ifdef ESP8266
//  Credits go: https://github.com/khoih-prog/ESP8266TimerInterrupt

// Select a Timer Clock
#define USING_TIM_DIV1 false   // for shortest and most accurate timer
#define USING_TIM_DIV16 true   // for medium time and medium accurate timer
#define USING_TIM_DIV256 false // for longest timer but least accurate. Default

#include "ESP8266TimerInterrupt.h"
#include "ESP8266_ISR_Timer.hpp"

#include <ESP8266_ISR_Timer.h>

// Init ESP8266 only and only Timer 1
ESP8266Timer ITimer1;

#else
// Select a Timer Clock
// Select the timers you're using, here ITimer1
#define USE_TIMER_1 true
#define USE_TIMER_2 false
#define USE_TIMER_3 false
#define USE_TIMER_4 false
#define USE_TIMER_5 false

//  Credits go: https://github.com/khoih-prog/ESP8266TimerInterrupt

#include "TimerInterrupt.h"
#endif

volatile uint8_t tx_rx_cycle = 0;

void MOVE2RAM TxTimerHandler()
{
    auto interrupt = tx_rx_cycle++ & 1 ? OnewireInterrupt::tx : OnewireInterrupt::rx;
    if (interrupt != nullptr)
        interrupt->timer_interrupt();
}

void OnewireInterrupt::kill()
{
    if (OnewireInterrupt::timer_attach_state >= 0)
    {
        ITimer1.detachInterrupt();
        OnewireInterrupt::timer_attach_state = -2;
    }
}

void OnewireInterrupt::attach()
{
    if (OnewireInterrupt::timer_attach_state >= 0)
        return;
#ifndef ESP8266
    ITimer1.init();
#endif

    // note we use attachInterrupt, instead of attachInterruptInterval since ESP library assumes micros,
    // while arduino assumes millis
    float delay = float((1000000L / (RX_BAUD << 1))) / 1000.0 / 1000.0; // in micros
    OnewireInterrupt::timer_attach_state = ITimer1.attachInterrupt(1.0 / float(delay), TxTimerHandler);
}

// to be refactored
void OnewireInterrupt::disableTimer()
{
    ITimer1.disableTimer();
}
void OnewireInterrupt::enableTimer()
{
    ITimer1.enableTimer();
}
#endif
