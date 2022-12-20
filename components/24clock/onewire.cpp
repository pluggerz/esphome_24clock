#include "onewire.h"

#ifdef ESP8266

// uint32_t *onewire::rx_basereg = PIN_TO_BASEREG(SYNC_IN_PIN);
// IO_REG_TYPE onewire::rx_bitmask = PIN_TO_BITMASK(SYNC_IN_PIN);

#include "esphome/core/log.h"
using namespace esphome;

const char *const TAG = "onewire";

#define DOLOG
#endif

using onewire::RxOnewire;
using onewire::TxOnewire;

#ifdef USE_RX_INTERRUPT
RxOnewire *rx_owner = nullptr;

#ifdef SLAVE
#define IRAM_ATTR
#endif // SLAVE

IRAM_ATTR void rx_handle_interrupt_rising()
{
    if (rx_owner == nullptr)
        return;
    rx_owner->handle_interrupt(true);
}

IRAM_ATTR void rx_handle_interrupt_falling()
{
    if (rx_owner == nullptr)
        return;
    rx_owner->handle_interrupt(false);
}

volatile Micros last = 0;
volatile bool nibble_state;
volatile int rise_counter = 0;
volatile int check_counter = 0;

void RxOnewire::handle_interrupt(bool rising)
{
    loop(micros());

    /*
        if (rising)
            rise_counter++;
        else
            rise_counter--;
        auto compare = read();

        if (compare != rising)
        {
            ESP_LOGW(TAG, "receive: rising = %d compare = %d -> %d", 0 + rising, 0 + compare, check_counter);
        }
        else
            check_counter++;
        if (check_counter > 100)
        {
            ESP_LOGW(TAG, "receive: RESET CHECK");
            check_counter = 0;
        }
        return;
        Micros now = micros();
        Micros delay = now - last;
        last = now;

        int nibbles = (delay + (_rx_delay >> 1)) / _rx_delay;
        int remainder = delay - nibbles * _rx_delay;
        int diff = 100 * abs(remainder) / _rx_delay;

        if (_rx_bit == RX_BIT_INITIAL)
        {
            // looking for a start
            if (rising == false && nibbles == 2)
            {
                _rx_bit = RX_BIT_INITIAL - 1;
                nibble_state = LOW;
                // ESP_LOGW(TAG, "receive: INITIAL");
            }
            // JUST FIND NEXT
            return;
        }
        if (_rx_bit == RX_BIT_INITIAL - 1)
        {
            if (rising == false)
            {
                // ESP_LOGW(TAG, "receive: RISING FALSE!?");
                reset(false);
                return;
            }
            bool is_end = 0 == (nibbles & 1);
            if (is_end)
            {
                // done
                // ESP_LOGI(TAG, "receive: END");
                reset(false);
                return;
            }
            int zeros = nibbles >> 1;
            _rx_value = 1 << zeros;
            _rx_bit = zeros;

            // ESP_LOGI(TAG, "receive: CURRENT: rx_value = %d", _rx_value);
            reset(false);
            return;
        }*/
}
#endif // USE_RX_INTERRUPT

void RxOnewire::begin(int baud)
{
    _rx_delay = 1000000L / baud;

#ifdef USE_RX_INTERRUPT
    rx_owner = this;
    attachInterrupt(digitalPinToInterrupt(SYNC_IN_PIN), rx_handle_interrupt_rising, CHANGE);
    attachInterrupt(digitalPinToInterrupt(SYNC_IN_PIN), rx_handle_interrupt_falling, FALLING);
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
        Leds::publish();
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

    _tx_t0 += _tx_delay;
    if (_tx_bit < 0)
    {
        bool bit = _tx_bit != -1;
        write(bit);
#ifdef DOLOG
        ESP_LOGV(TAG, "transmit: START %s tx_value=%d, tx_bit=%d, delta=%d, pointer=%f)", bit ? "HIGH" : "LOW ", _tx_value, _tx_bit, delta, transmit_pointer());
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
        ESP_LOGV(TAG, "transmit: DATA%s %s tx_value=%d, tx_bit=%d, delta=%d, pointer=%f)", _tx_nibble ? "S" : "F", written ? "HIGH" : "LOW ", _tx_value, _tx_bit, delta, transmit_pointer());
#endif
        if (_tx_nibble)
        {
            _tx_bit = _tx_remainder_value == 0 ? MAX_DATA_BITS : _tx_bit + 1;
        }
        _tx_nibble = !_tx_nibble;
        return;
    }

    if (_tx_bit == MAX_DATA_BITS)
    {
#ifdef DOLOG
        ESP_LOGV(TAG, "transmit: END1  LOW  tx_value=%d, tx_bit=%d, delta=%d, pointer=%f)", _tx_value, _tx_bit, delta, transmit_pointer());
#endif

        write(false);
        _tx_bit++;
    }
    else if (_tx_bit == MAX_DATA_BITS + 1)
    {
#ifdef DOLOG
        ESP_LOGV(TAG, "transmit: END2  HIGH tx_value=%d, tx_bit=%d, delta=%d, pointer=%f)", _tx_value, _tx_bit, delta, transmit_pointer());
#endif
        write(true);

        _tx_bit++;
    }
    else if (_tx_bit == MAX_DATA_BITS + 2)
    {
#ifdef DOLOG
        ESP_LOGV(TAG, "transmit: END3  LOW tx_value=%d, tx_bit=%d, delta=%d, pointer=%f)", _tx_value, _tx_bit, delta, transmit_pointer());
#endif
        write(false);

        _tx_bit++;
    }
    else if (_tx_bit == MAX_DATA_BITS + 3)
    {
#ifdef DOLOG
        ESP_LOGV(TAG, "transmit: FINAL tx_value=%d, tx_bit=%d, delta=%d, pointer=%f)", _tx_value, _tx_bit, delta, transmit_pointer());
        ESP_LOGD(TAG, "transmit: END tx_value=%d", _tx_value);
#endif
        _tx_bit = LAST_TX_BIT;
    }
}

/* TODO: remove
void OneWireProtocol::loop()
{
    Micros now = micros();
    TxOnewire::loop(now);
    RxOnewire::loop(now);
}*/

void TxOnewire::transmit(onewire::Value value)
{
#ifdef DOLOG
    if (_tx_bit != LAST_TX_BIT && _tx_bit != LAST_TX_BIT + 1)
    {
        ESP_LOGW(TAG, "transmit not complete !?: tx_value=%d, tx_bit=%d, pointre=%f)", _tx_value, _tx_bit, transmit_pointer());
    }
#endif

    _tx_nibble = false;
    _tx_bit = -onewire::START_BITS;
    _tx_value = value & onewire::DATA_MASK;
    _tx_remainder_value = _tx_value;
    _tx_tstart = _tx_t0 = micros();
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

    if (async)
    {
        write(true);
    }
    else
    {
        // noInterrupts();
        write(true);
        while (!transmitted())
        {
            loop(micros());
            //  yield();
        }
        // interrupts();
    }
}
