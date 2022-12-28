#include "onewire.h"

#ifdef ESP8266

#include "esphome/core/log.h"
using namespace esphome;

const char *const TAG = "1wireTX";

#define DOLOG
#endif

using onewire::OnewireInterrupt;
using onewire::RawTxOnewire;
using onewire::Tx;
using onewire::TxOnewire;

TxOnewire *OnewireInterrupt::tx = nullptr;

void RawTxOnewire::setup()
{
    Tx::setup();

    OnewireInterrupt::attach();
}

void RawTxOnewire::dump_config()
{
#ifdef DOLOG
    ESP_LOGCONFIG(TAG, "  TxOnewire");
    ESP_LOGCONFIG(TAG, "     re_pin: %d", RS485_RE_PIN);
    ESP_LOGCONFIG(TAG, "     OnewireInterrupt::tx: 0x%d", OnewireInterrupt::tx);
    // ESP_LOGCONFIG(TAG, "     state: %d", state);
#endif
}

void RawTxOnewire::timer_interrupt()
{
    if (_tx_bit == LAST_TX_BIT + 4)
    {
        // OnewireInterrupt::disableTimer();
        // done
        return;
    }
    write_to_sync();
    return;
}

void RawTxOnewire::write_to_sync()
{
    if (_tx_bit == -onewire::START_BITS - 1)
    {
        _tx_bit++;
        write(true);
        return;
    }

    if (_tx_bit < 0)
    {
        bool bit = _tx_bit != -1;
        write(bit);
#ifdef DOLOG
        ESP_LOGV(TAG, "transmit: START %s tx_value=%d, tx_bit=%d)", bit ? "HIGH" : "LOW ", _tx_value, _tx_bit);
#endif
        _tx_bit++;
        return;
    }

    if (_tx_bit < MAX_DATA_BITS)
    {
        onewire::Value mask = onewire::Value(1) << onewire::Value(_tx_bit);
        bool bit = _tx_remainder_value & mask;
        // if (bit && !_tx_nibble)
        bool written = bit && !_tx_nibble;
        write(written);
        if (written)
        {
            _tx_transmitted_value |= mask;
            _tx_remainder_value -= mask;
        }
#ifdef DOLOG
        ESP_LOGV(TAG, "transmit: DATA%s %s tx_value=%d, _tx_transmitted=%d, tx_bit=%d)", _tx_nibble ? "S" : "F", written ? "HIGH" : "LOW ", _tx_value, _tx_transmitted_value, _tx_bit);
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
        ESP_LOGV(TAG, "transmit: END1  LOW  tx_value=%d, tx_bit=%d)", _tx_value, _tx_bit);
#endif

        write(false);
        _tx_bit++;
    }
    else if (_tx_bit == MAX_DATA_BITS + 1)
    {
#ifdef DOLOG
        ESP_LOGV(TAG, "transmit: END2  HIGH tx_value=%d, tx_bit=%d)", _tx_value, _tx_bit);
#endif
        write(true);

        _tx_bit++;
    }
    else if (_tx_bit == MAX_DATA_BITS + 2)
    {
#ifdef DOLOG
        ESP_LOGV(TAG, "transmit: END3  LOW tx_value=%d, tx_bit=%d)", _tx_value, _tx_bit);
#endif
        write(false);

        _tx_bit++;
    }
    else if (_tx_bit == MAX_DATA_BITS + 3)
    {
#ifdef DOLOG
        ESP_LOGV(TAG, "transmit: FINAL tx_value=%d, tx_bit=%d)", _tx_value, _tx_bit);
        ESP_LOGD(TAG, "transmit: END tx_value=%d", _tx_value);
#endif
        _tx_bit = LAST_TX_BIT;
    }
}

void RawTxOnewire::transmit(onewire::Value value)
{

    if (_tx_delay == 0)
    {
        return;
    }
    onewire::Value masked_value = value & DATA_MASK;
    if (masked_value != value)
    {
#ifdef DOLOG
        ESP_LOGW(TAG, "Too many bits, data will be lost!? value=%d, max_bits=%d", value, MAX_DATA_BITS);
#endif
    }

    // OnewireInterrupt::disableTimer();
#ifdef DOLOG
    if (_tx_bit != LAST_TX_BIT && _tx_bit != LAST_TX_BIT + 1)
    {
        ESP_LOGW(TAG, "transmit not complete !?: tx_value=%d, tx_bit=%d)", _tx_value, _tx_bit);
    }
#endif
    _tx_bit = LAST_TX_BIT;

    _tx_nibble = false;
    _tx_value = masked_value;
    _tx_remainder_value = _tx_value;
    _tx_transmitted_value = 0;

    // this one should be done last
    _tx_bit = -onewire::START_BITS - 1;
#ifdef DOLOG
    if (_tx_value != value)
    {
        ESP_LOGW(TAG, "Warning masked value(=%d) is not same as input(=%d)", _tx_value, value);
    }
#endif
#ifdef DOLOG
    ESP_LOGD(TAG, "transmit: START tx_value=%d", _tx_value);
    ESP_LOGV(TAG, "transmit: START HIGH tx_value=%d, tx_bit=%d)", _tx_value, _tx_bit);
#endif
    // OnewireInterrupt::enableTimer();
}

void RawTxOnewire::kill()
{
#ifdef DOLOG
    ESP_LOGW(TAG, "kill()");
#endif
    _tx_delay = 0;
    OnewireInterrupt::kill();
}