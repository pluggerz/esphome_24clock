#include "onewire.h"

#ifdef ESP8266

#include "esphome/core/log.h"
using namespace esphome;

const char *const TAG = "1wireTX";

#define DOLOG
#endif

using onewire::Tx;
using onewire::TxOnewire;

int tx_succeeded = -2;
volatile uint32_t tx_first_time = 0;
volatile uint32_t tx_last_time = 0;
volatile uint32_t tx_total_time = 0;
volatile uint32_t tx_ticks = 0;

#ifdef TX_TIMER
//  Credits go: https://github.com/khoih-prog/ESP8266TimerInterrupt

// Select a Timer Clock
#define USING_TIM_DIV1 false   // for shortest and most accurate timer
#define USING_TIM_DIV16 true   // for medium time and medium accurate timer
#define USING_TIM_DIV256 false // for longest timer but least accurate. Default

#include "ESP8266TimerInterrupt.h"
#include "ESP8266_ISR_Timer.hpp"

#include <ESP8266_ISR_Timer.h>

// Init ESP8266 only and only Timer 1
ESP8266Timer ITimer;

TxOnewire *tx_timer = nullptr;
void IRAM_ATTR TxTimerHandler()
{
    //  ESP_LOGI(TAG, "next!");
    tx_timer->timer_loop();
}
#endif

void TxOnewire::setup()
{
    Tx::setup();

#ifdef TX_TIMER
    tx_succeeded = ITimer.attachInterruptInterval(_tx_delay, TxTimerHandler);
    tx_timer = this;
#endif // TX_TIMER
}

void TxOnewire::dump_config()
{
#ifdef DOLOG
    ESP_LOGCONFIG(TAG, "  TxOnewire");
    ESP_LOGCONFIG(TAG, "     tx_succeeded: %d", tx_succeeded);
    ESP_LOGCONFIG(TAG, "     re_pin: %d", RS485_RE_PIN);
    ESP_LOGCONFIG(TAG, "     state: %d", state);
#endif
}

#ifdef TX_TIMER
void TxOnewire::timer_loop()
{
    if (_tx_bit == LAST_TX_BIT + 4)
    {
        ITimer.disableTimer();
        // done
        return;
    }
    write_to_sync();
}
#endif

#ifndef TX_TIMER
void TxOnewire::loop(Micros now)
{
    if (_tx_bit == LAST_TX_BIT + 4)
    {
        // done
        return;
    }

    const auto delta = now - _tx_t0;
    if (delta < _tx_delay)
        return;

    write_to_sync();
}
#endif

void TxOnewire::write_to_sync()
{
    // for checking actual bitrate
    auto now = micros();
    if (tx_last_time != 0 && _tx_tstart != 0)
    {
        auto delay = now - tx_last_time;
        tx_ticks++;
        tx_total_time += delay;
    }
    tx_last_time = now;

    if (_tx_bit == -onewire::START_BITS - 1)
    {
        _tx_tstart = micros();
#ifndef TX_TIMER
        _tx_t0 = _tx_tstart;
#endif
        _tx_bit++;
        write(true);
        return;
    }

#ifndef TX_TIMER
    _tx_t0 += _tx_delay;
#endif
    if (_tx_bit < 0)
    {
        bool bit = _tx_bit != -1;
        write(bit);
#ifdef DOLOG
        ESP_LOGV(TAG, "transmit: START %s tx_value=%d, tx_bit=%d, pointer=%f)", bit ? "HIGH" : "LOW ", _tx_value, _tx_bit, transmit_pointer());
#endif
        _tx_bit++;
        return;
    }

    if (_tx_bit < MAX_DATA_BITS)
    {
        onewire::Value mask = 1 << _tx_bit;
        bool bit = _tx_remainder_value & mask;
        // if (bit && !_tx_nibble)
        bool written = bit && !_tx_nibble;
        write(written);
        if (written)
        {
            _tx_remainder_value -= mask;
        }
#ifdef DOLOG
        ESP_LOGV(TAG, "transmit: DATA%s %s tx_value=%d, tx_bit=%d, pointer=%f)", _tx_nibble ? "S" : "F", written ? "HIGH" : "LOW ", _tx_value, _tx_bit, transmit_pointer());
#endif
        if (_tx_nibble)
        {
            _tx_bit = _tx_remainder_value == 0 ? MAX_DATA_BITS : _tx_bit + 1;
            //_tx_bit++;
        }
        _tx_nibble = !_tx_nibble;
        return;
    }

    if (_tx_bit == MAX_DATA_BITS)
    {
#ifdef DOLOG
        ESP_LOGV(TAG, "transmit: END1  LOW  tx_value=%d, tx_bit=%d, pointer=%f)", _tx_value, _tx_bit, transmit_pointer());
#endif

        write(false);
        _tx_bit++;
    }
    else if (_tx_bit == MAX_DATA_BITS + 1)
    {
#ifdef DOLOG
        ESP_LOGV(TAG, "transmit: END2  HIGH tx_value=%d, tx_bit=%d, pointer=%f)", _tx_value, _tx_bit, transmit_pointer());
#endif
        write(true);

        _tx_bit++;
    }
    else if (_tx_bit == MAX_DATA_BITS + 2)
    {
#ifdef DOLOG
        ESP_LOGV(TAG, "transmit: END3  LOW tx_value=%d, tx_bit=%d, pointer=%f)", _tx_value, _tx_bit, transmit_pointer());
#endif
        write(false);

        _tx_bit++;
    }
    else if (_tx_bit == MAX_DATA_BITS + 3)
    {
#ifdef DOLOG
        ESP_LOGV(TAG, "transmit: FINAL tx_value=%d, tx_bit=%d, pointer=%f)", _tx_value, _tx_bit, transmit_pointer());
        ESP_LOGD(TAG, "transmit: END tx_value=%d", _tx_value);
#endif
        _tx_bit = LAST_TX_BIT;
    }
}

void TxOnewire::transmit(onewire::Value value)
{
    if (_tx_delay == 0)
    {
        return;
    }
#ifdef TX_TIMER
    ITimer.disableTimer();
#endif
    if (tx_ticks > 0)
    {
#ifdef DOLOG
        ESP_LOGD(TAG, "tx_ticks=%d tx_total_time=%d delay=%d, average=%f missed: %d", tx_ticks, tx_total_time, _tx_delay, float(tx_total_time) / tx_ticks, tx_last_time - tx_first_time - tx_total_time);
#endif
        tx_ticks = 0;
        tx_total_time = 0;
    }
    tx_first_time = tx_last_time = 0;
#ifdef DOLOG
    if (_tx_bit != LAST_TX_BIT && _tx_bit != LAST_TX_BIT + 1)
    {
        ESP_LOGW(TAG, "transmit not complete !?: tx_value=%d, tx_bit=%d, pointre=%f)", _tx_value, _tx_bit, transmit_pointer());
    }
#endif

    _tx_nibble = false;
    _tx_bit = -onewire::START_BITS - 1;
    _tx_value = value & onewire::DATA_MASK;
    _tx_remainder_value = _tx_value;
    _tx_tstart = 0L;
#ifdef DOLOG
    if (_tx_value != value)
    {
        ESP_LOGW(TAG, "Warning masked value(=%d) is not same as input(=%d), pointer=%f", _tx_value, value, transmit_pointer());
    }
#endif
#ifdef DOLOG
    ESP_LOGD(TAG, "transmit: START tx_value=%d", _tx_value);
    ESP_LOGV(TAG, "transmit: START HIGH tx_value=%d, tx_bit=%d, pointer=%f)", _tx_value, _tx_bit, transmit_pointer());
#endif
    // reset(true);

    // write(true);
#ifdef TX_TIMER
    ITimer.enableTimer();
#endif
}

void TxOnewire::kill()
{
#ifdef DOLOG
    ESP_LOGW(TAG, "kill()");
#endif
    _tx_delay = 0;
#ifdef TX_TIMER
    ITimer.detachInterrupt();
#endif
}