#include "onewire.h"

#ifdef TX_TIMER

using onewire::TimerInterrupt;

TimerInterrupt *TimerInterrupt::rx = nullptr;
TimerInterrupt *TimerInterrupt::tx = nullptr;

int TimerInterrupt::timer_attach_state = -2;

// Select a Timer Clock
#define USING_TIM_DIV1 false   // for shortest and most accurate timer
#define USING_TIM_DIV16 true   // for medium time and medium accurate timer
#define USING_TIM_DIV256 false // for longest timer but least accurate. Default

//  Credits go: https://github.com/khoih-prog/ESP8266TimerInterrupt

#include "ESP8266TimerInterrupt.h"
#include "ESP8266_ISR_Timer.hpp"

#include <ESP8266_ISR_Timer.h>

// Init ESP8266 only and only Timer 1
ESP8266Timer ITimer;
uint8_t tx_rx_cycle = 0;

void IRAM_ATTR TxTimerHandler()
{
    tx_rx_cycle++;
    auto interrupt = tx_rx_cycle & 1 ? TimerInterrupt::tx : TimerInterrupt::rx;
    if (interrupt != nullptr)
        interrupt->timer_interrupt();
}

void TimerInterrupt::kill()
{
    if (TimerInterrupt::timer_attach_state >= 0)
    {
        ITimer.detachInterrupt();
        TimerInterrupt::timer_attach_state = -2;
    }
}

void TimerInterrupt::attach()
{
    if (TimerInterrupt::timer_attach_state >= 0)
        return;
    auto delay = (1000000L / RX_BAUD);
    TimerInterrupt::timer_attach_state = ITimer.attachInterruptInterval(delay >> 1, TxTimerHandler);
}

// to be refactored
void TimerInterrupt::disableTimer()
{
    ITimer.disableTimer();
}
void TimerInterrupt::enableTimer()
{
    ITimer.enableTimer();
}
#endif