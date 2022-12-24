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
                ESP_LOGV(TAG, "receive:  Possible END");
            }
            else
            {
                // invalid state!
                ESP_LOGW(TAG, "receive:  INVALID !?");
                reset_interrupt();
            }
            return;
        }
        else
        {
            if (current)
                _rx_value |= 1 << _rx_bit;
            ESP_LOGV(TAG, "receive:  DATAF: %s @%d (%d)", current ? "HIGH" : "LOW", _rx_bit, _rx_value);
            _rx_nibble = true;
            return;
        }
    }

    switch (_rx_bit)
    {
    case MAX_DATA_BITS + 2:
        if (current)
            ESP_LOGI(TAG, "receive:  INVALIF END !?");
        else
        {
            ESP_LOGD(TAG, "receive:  END -> %d", _rx_value);
            _rx_last_value = _rx_value;
            _rx_available = true;
        }
        reset_interrupt();
        return;

    case MAX_DATA_BITS + 1:
    case MAX_DATA_BITS:
        if (current != (_rx_bit == MAX_DATA_BITS ? false : true))
        {
            ESP_LOGI(TAG, "NOT EXPECTED !?");
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
            ESP_LOGV(TAG, "receive:  START DETECTED");
            _rx_bit = 0;
            _rx_nibble = false;
            _rx_value = 0;
            return;
        }
        else if (current && last)
            _rx_nibble = true;
        return;

    default:
        ESP_LOGV(TAG, "receive:  UNKNOWN STATE!?");
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

void RxOnewire::begin(int baud)
{
    _rx_delay = 1000000L / baud;

    OnewireInterrupt::attach();
    OnewireInterrupt::rx = this;

#ifdef USE_RX_INTERRUPT
    rx_owner = this;
    attachInterrupt(digitalPinToInterrupt(SYNC_IN_PIN), rx_handle_change, CHANGE);
#endif

    reset(false);
#ifdef DOLOG
    ESP_LOGI(TAG, "OneWireProtocol: %dbaud %d delay", baud, _rx_delay);
#endif
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
    _rx_bit = RX_BIT_INITIAL;
    _rx_nibble = false;

#ifdef DOLED
    Leds::set(1, rgb_color(0, 0, 0xFF));
#endif
}
