#include "onewire.h"

#ifdef ESP8266

#include "esphome/core/log.h"
using namespace esphome;

const char *const TAG = "1wireRX";

#define DOLOG
#endif

using onewire::RxOnewire;

#ifdef USE_RX_INTERRUPT
RxOnewire *rx_owner = nullptr;

#ifdef SLAVE
#define IRAM_ATTR
#endif // SLAVE

MOVE2RAM void rx_handle_change()
{
    if (rx_owner == nullptr)
        return;
    rx_owner->handle_interrupt(PIN_READ(SYNC_IN_PIN));
}

int intr = 0;
void RxOnewire::handle_interrupt(bool state)
{
#ifdef DOLOG
    if (intr++ > 1000)
    {
        ESP_LOGI(TAG, "handle_interrupt **");
        intr = 0;
    }

#endif
}

#endif // USE_RX_INTERRUPT

void RxOnewire::begin(int baud)
{
    _rx_delay = 1000000L / baud;

#ifdef USE_RX_INTERRUPT
    rx_owner = this;
    attachInterrupt(digitalPinToInterrupt(SYNC_IN_PIN), rx_handle_change, CHANGE);
#endif

    reset(false);
#ifdef DOLOG
    ESP_LOGI(TAG, "OneWireProtocol: %dbaud %d delay", baud, _rx_delay);
#endif
}

void RxOnewire::loop(Micros now)
{
    if (_rx_delay == 0)
        // forgot to call begin?
        return;

    if (_rx_loop)
    {
        return;
    }
    _rx_loop = true;
    inner_loop(now);
    _rx_loop = false;
}

void RxOnewire::inner_loop(Micros now)
{
    if (_rx_bit == RX_BIT_INITIAL)
    {
        // looking for a start
        if (read())
        {
            // possible start
            _rx_tstart = now;
            _rx_t0 = 0;

#ifdef DOLOG
            ESP_LOGVV(TAG, "receive:  ENTRY HIGH rx_value=%d, rx_bit=%d, pointer=%f)", _rx_value, _rx_bit, recieve_pointer());
#endif
#ifdef DOLED
            Leds::set(1, rgb_color(0xFF, 0xFF, 0x00));
#endif
            _rx_bit++;
        }
        return;
    }
    if (_rx_t0 == 0)
    {
        // looking for a end
        if (read())
            return;

        int raw_delta = now - _rx_tstart;
        int expected_delta = _rx_delay * onewire::START_BITS;
        int diff_in_perc = 100 * abs(expected_delta - raw_delta) / expected_delta;
#ifdef DOLOG
        int delta = raw_delta / onewire::START_BITS;
        ESP_LOGV(TAG, "receive:  START LOW rx_value=%d, rx_bit=%d, delta=%d, diff_in_perc=%d, pointer=%f)", _rx_value, _rx_bit, delta, diff_in_perc, recieve_pointer());
#endif
        if (diff_in_perc > 10)
        {
// #ifdef DOLOG
//             ESP_LOGW(TAG, "receive:  check: raw_delta=%d raw_delta/startbits=%d expected=%d diff_in_perc=%d pointer=%f", raw_delta, delta, _rx_delay, diff_in_perc, recieve_pointer());
// #endif
#ifdef DOLED
            Leds::set(1, rgb_color(0xFF, 0x00, 0x00));
            Leds::set(LED_COUNT - 6, rgb_color(0x00, 0x00, 0xFF));
#endif
            // #ifdef DOLOG
            //             ESP_LOGV(TAG, "receive:  IGNORE rx_value=%d, rx_bit=%d, delta=%d, diff_in_perc=%d, pointer=%f)", _rx_value, _rx_bit, delta, diff_in_perc, recieve_pointer());
            // #endif
            //  ignore
            _rx_bit = RX_BIT_INITIAL;
            return;
        }
#ifdef DOLED
        Leds::set(1, rgb_color(0x00, 0xFF, 0x00));
        Leds::set(LED_COUNT - 6, rgb_color(0xFF, 0xFF, 0x00));
#endif
        _rx_t0 = now;
        return;
    }

    // first bit is special
    if (_rx_bit == -onewire::START_BITS)
    {
        if (now - _rx_t0 < (_rx_delay >> 1))
        {
            // ignore for first snapshot
            return;
        }
        _rx_t0 = now;

        bool state = read();

#ifdef DOLOG
        ESP_LOGV(TAG, "receive:   PRE   %s rx_value=%d, rx_bit=%d, pointer=%f)", state ? "HIGH" : "LOW", _rx_value, _rx_bit, recieve_pointer());
#endif
        if (state)
        {
            reset(true);
            return;
        }
        _rx_bit = 0;
        return;
    }

    // read data
    const auto delta = now - _rx_t0;
    if (delta < _rx_delay)
    {
        // ignore for first snapshot
        return;
    }
    _rx_t0 += _rx_delay;

    bool state = read();
    if (_rx_bit < MAX_DATA_BITS)
    {
#ifdef DOLOG
        ESP_LOGV(TAG, "receive:  DATA%s %s rx_value=%d, rx_bit=%d, delta=%d, pointer=%f)", _rx_nibble ? "S" : "F", state ? "HIGH" : "LOW ", _rx_value, _rx_bit, delta, recieve_pointer());
#endif
        if (_rx_nibble && state)
        {
            // end bit ?
            if (last_state() == false)
            {
                // possible end
#ifdef DOLOG
                ESP_LOGV(TAG, "receive:  ACTUALLY END BIT");
#endif
                _rx_bit = MAX_DATA_BITS + 2;
                return;
            }
#ifdef DOLOG
            ESP_LOGV(TAG, "receive:  INVALID NIBBLE");
#endif
            reset(true);
        }
        if (!_rx_nibble && state)
            _rx_value |= 1 << _rx_bit;
        if (_rx_nibble)
            _rx_bit++;
        _rx_nibble = !_rx_nibble;
        return;
    }
    if (_rx_bit == MAX_DATA_BITS)
    {
#ifdef DOLOG
        ESP_LOGV(TAG, "receive:  END0 %s rx_value=%d, rx_bit=%d, delta=%d, pointer=%f)", state ? "HIGH" : "LOW ", _rx_value, _rx_bit, delta, recieve_pointer());
#endif
        _rx_bit++;

        if (state)
        {
#ifdef DOLOG
            ESP_LOGV(TAG, "INVALID STATE");
#endif
            reset(true);
        }
        return;
    }

    if (_rx_bit == MAX_DATA_BITS + 1)
    {
#ifdef DOLOG
        ESP_LOGV(TAG, "receive:  END1 %s rx_value=%d, rx_bit=%d, delta=%d, pointer=%f)", state ? "HIGH" : "LOW ", _rx_value, _rx_bit, delta, recieve_pointer());
#endif
        _rx_bit++;

        if (!state)
        {
#ifdef DOLOG
            ESP_LOGV(TAG, "INVALID STATE");
#endif
            reset(true);
        }
        return;
    }

    if (_rx_bit == MAX_DATA_BITS + 2)
    {
#ifdef DOLOG
        ESP_LOGV(TAG, "receive:  END2 %s rx_value=%d, rx_bit=%d, delta=%d, pointer=%f)", state ? "HIGH" : "LOW ", _rx_value, _rx_bit, delta, recieve_pointer());
#endif
        _rx_bit++;

        if (state)
        {
#ifdef DOLOG
            ESP_LOGV(TAG, "INVALID STATE");
#endif
            reset(true);
        }

#ifdef DOLOG
        ESP_LOGD(TAG, "received: GOT rx_value=%d", _rx_value);
#endif
        _rx_last_value = _rx_value;
        _rx_available = true;
        reset(false);
#ifdef DOLED
        Leds::set(1, rgb_color(0xFF, 0x00, 0xFF));
#endif
        return;
    }

#ifdef DOLOG
    ESP_LOGV(TAG, "OOOH... INVALID STATE");
#endif
    reset(true);
}

void onewire::RxOnewire::reset(bool forced)
{
    if (forced && _rx_bit != RX_BIT_INITIAL)
    {
#ifdef DOLED
        Leds::set(LED_COUNT - 6, rgb_color(0xFF, 0x00, 0x00));
        // Leds::publish();
#endif
#ifdef DOLOG
        ESP_LOGW(TAG, "receive: RESET rx_value=%d, rx_bit=%d) -> pre start bit", _rx_value, _rx_bit);
#endif
    }
    else
    {
#ifdef DOLED
        Leds::set(LED_COUNT - 6, rgb_color(0x00, 0xFF, 0x00));
#endif
    }
    _rx_value = 0;
    _rx_tstart = 0;
    _rx_t0 = 0;
    _rx_bit = RX_BIT_INITIAL;
    _rx_nibble = false;

#ifdef DOLED
    Leds::set(1, rgb_color(0, 0, 0xFF));
#endif
}
