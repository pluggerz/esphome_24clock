#include "onewire.h"

using onewire::OnewireInterrupt;
using onewire::RxOnewire;

#ifdef ESP8266

#include "esphome/core/log.h"

using namespace esphome;

const char *const TAG = "1wireRX";

#define DOLOG
#endif

RxOnewire *OnewireInterrupt::rx = nullptr;

void RxOnewire::timer_interrupt()
{
    auto current = PIN_READ(SYNC_IN_PIN);
    timer_interrupt(_rx_last_state, current);
    _rx_last_state = current;
}

void RxOnewire::timer_interrupt(bool last, bool current)
{
    if (_rx_bit >= 0 && _rx_bit < MAX_DATA_BITS)
    {
        if (_rx_nibble)
        {
            _rx_nibble = false;
            if (!current) // databit
                _rx_bit++;
            else if (current && !last)
            { // actual end symbol
                _rx_bit = MAX_DATA_BITS + 2;
#ifdef DOLOG
                ESP_LOGV(TAG, "receive:  Possible END");
#endif
            }
            else
            {
// invalid state!
#ifdef DOLOG
                ESP_LOGW(TAG, "receive:  INVALID !?");
#endif
                reset_interrupt();
            }
            return;
        }
        else
        {
            if (current)
                _rx_value |= onewire::Value(1) << onewire::Value(_rx_bit);
#ifdef DOLOG
            ESP_LOGV(TAG, "receive:  DATAF: %s @%d (%d)", current ? "HIGH" : "LOW", _rx_bit, _rx_value);
#endif
            _rx_nibble = true;
            return;
        }
    }

    switch (_rx_bit)
    {
    case MAX_DATA_BITS + 2:
        if (current)
        {
#ifdef DOLOG
            ESP_LOGI(TAG, "receive:  INVALIF END !?");
#endif
        }
        else
        {
#ifdef DOLOG
            ESP_LOGD(TAG, "receive:  END -> %d", _rx_value);
#endif
            _rx_last_value = _rx_value;
            _rx_available = true;
#ifdef DOLED
            Leds::set_ex(LED_ONEWIRE, LedColors::green);
#endif
        }
        reset_interrupt();
        return;

    case MAX_DATA_BITS + 1:
    case MAX_DATA_BITS:
        if (current != (_rx_bit == MAX_DATA_BITS ? false : true))
        {
#ifdef DOLOG
            ESP_LOGI(TAG, "NOT EXPECTED !?");
#endif
            reset_interrupt();
        }
        else
            _rx_bit++;
        return;

    case onewire::RX_BIT_INITIAL:
        if (_rx_nibble)
        {
            if (current)
            {
                // snipz... reset
                // ESP_LOGI(TAG, "RESET!");
                reset_interrupt();
                return;
            }
#ifdef DOLOG
            ESP_LOGV(TAG, "receive:  START DETECTED");
#endif
            _rx_bit = 0;
            _rx_nibble = false;
            _rx_value = 0;
#ifdef DOLED
            Leds::set_ex(LED_ONEWIRE, LedColors::blue);
#endif
            return;
        }
        else if (current && last)
            _rx_nibble = true;
        return;

    default:
#ifdef DOLOG
        ESP_LOGV(TAG, "receive:  UNKNOWN STATE!?");
#endif
        reset_interrupt();
    }
}

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

void RxOnewire::begin()
{
    OnewireInterrupt::attach();
    OnewireInterrupt::rx = this;

#ifdef USE_RX_INTERRUPT
    rx_owner = this;
    attachInterrupt(digitalPinToInterrupt(SYNC_IN_PIN), rx_handle_change, CHANGE);
#endif

    reset(false);
#ifdef DOLOG
    ESP_LOGI(TAG, "OneWireProtocol: %dbaud", BAUD);
#endif
}

void onewire::RxOnewire::reset(bool forced)
{
    if (forced && _rx_bit != RX_BIT_INITIAL)
    {
#ifdef DOLED
        Leds::set_ex(LED_ONEWIRE, LedColors::red);
#endif
#ifdef DOLOG
        ESP_LOGW(TAG, "receive: RESET rx_value=%d, rx_bit=%d) -> pre start bit", _rx_value, _rx_bit);
#endif
    }
    else
    {
#ifdef DOLED
        //    Leds::set_ex(LED_ONEWIRE, LedColors::blue);
#endif
    }
    _rx_value = 0;
    _rx_bit = RX_BIT_INITIAL;
    _rx_nibble = false;
}
