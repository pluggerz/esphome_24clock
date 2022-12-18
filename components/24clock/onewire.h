#ifndef ONEWIRE_H
#define ONEWIRE_H

#include "stub.h"

#include "pins.h"
#include "ringbuffer.h"

#ifdef SLAVE
#define DOLED
#endif

#ifdef DOLED
#include "slave/leds.h"
#endif

constexpr int8_t MAX_DATA_BITS = 32;

#define setPin(b) ((b) < 8 ? PORTD |= (1 << (b)) : PORTB |= (1 << (b - 8)))

#define clrPin(b) ((b) < 8 ? PORTD &= ~(1 << (b)) : PORTB &= ~(1 << (b - 8)))

#define tstPin(b) ((b) < 8 ? (PORTD & (1 << (b))) != 0 : (PORTB & (1 << (b - 8))) != 0)

namespace onewire
{
    // for now globally fixed
    constexpr uint32_t BAUD = 50;
    constexpr uint32_t _tx_delay = 1000000L / BAUD;
    constexpr int8_t START_BITS = 2;

    typedef uint16_t Value;
    const Value DATA_MASK = ~Value(0);

    class Rx
    {
        bool _rx_last_state = false, _rx_state = false;

#ifdef DOLED
        int last_read = -1;
#endif
    public:
        bool async = true;

    protected:
        bool last_state() const
        {
            return _rx_last_state;
        }

        bool read() __attribute__((always_inline))
        {
            _rx_last_state = _rx_state;
#ifdef USE_FAST_GPIO
            bool state = retFastGPIO::Pin<SYNC_IN_PIN>::isInputHigh();
#else
            _rx_state = digitalRead(SYNC_IN_PIN) == HIGH;
#endif
#ifdef DOLED
            if ((_rx_state ? 1 : 0) != last_read)
            {
                last_read = _rx_state ? 1 : 0;
                Leds::set(0, _rx_state ? rgb_color(0xFF, 0, 0) : rgb_color(0x00, 0xFF, 0));
            }
#endif
            return _rx_state;
        }

    public:
        void setup()
        {
            pinMode(SYNC_IN_PIN, INPUT);
        }
    };

    class Tx
    {
#ifdef DOLED
        int last_written = -1;
#endif

    public:
        bool async = true;
        void setup()
        {
            pinMode(SYNC_OUT_PIN, OUTPUT);
            write(false);
        }

        void write(bool state) __attribute__((always_inline))
        {
#ifdef USE_FAST_GPIO
            FastGPIO::Pin<SYNC_OUT_PIN>::setOutputValue(state);
#else
            digitalWrite(SYNC_OUT_PIN, state ? HIGH : LOW);
#endif
#ifdef DOLED
            if ((state ? 1 : 0) != last_written)
            {
                last_written = state ? 1 : 0;
                Leds::set(2, state ? rgb_color(0xFF, 0, 0) : rgb_color(0x00, 0xFF, 0));
            }
#endif
        }
    };

    /**
     * @brief
     *
     *  delay is determined by BAUD,
     * For now:
     * - START_BITS times high
     * - 1 time low
     * - 0/1 depending on DATABITS
     * - 2 times low
     */
    class RxOnewire : public onewire::Rx
    {
    protected:
        Micros _rx_t0;
        Micros _rx_tstart;

        onewire::Value _rx_last_value;

        bool _rx_nibble = false;
        int8_t _rx_bit = RX_BIT_INITIAL;
        bool _rx_available = false;

        onewire::Value _rx_value;

        const int8_t RX_BIT_INITIAL = -onewire::START_BITS - 1;

        void reset(bool forced);

        float recieve_pointer() const
        {
            return float(micros()) / float(onewire::_tx_delay);
            // return (micros() - float(_rx_tstart)) / float(_tx_delay);
        }

    public:
        /***
         * pending:
         * true: a byte is available
         * false: no byte is available
         */
        bool pending() const
        {
            return _rx_available;
        }

        /***
         * if pending() == true, then returns the read byte, otherwise -1
         */
        onewire::Value flush()
        {
            _rx_available = false;
            return _rx_last_value;
        }

        void begin()
        {
            reset(false);
#ifdef DOLOG
            ESP_LOGI(TAG, "OneWireProtocol: %d baud %d delay", onewire::BAUD, onewire::_tx_delay);
#endif
        }
        void loop(Micros now);
    };

    class TxOnewire : public onewire::Tx
    {
    protected:
        Micros _tx_t0;
        Micros _tx_tstart;

        int8_t _tx_bit = LAST_TX_BIT;
        bool _tx_nibble = false;

        onewire::Value _tx_value, _tx_remainder_value;

        const int8_t LAST_TX_BIT = MAX_DATA_BITS + 4;

        float transmit_pointer() const
        {
            // return float(micros()) / float(_tx_delay);
            return (micros() - float(_tx_tstart)) / float(onewire::_tx_delay);
        }

    public:
        void transmit(onewire::Value value);

        bool transmitted() const
        {
            return _tx_bit == MAX_DATA_BITS + 4;
        }
        void loop(Micros now);
    };

    template <uint16_t SIZE>
    class BufferedTxOnewire
    {
        typedef RingBuffer<onewire::Value, SIZE> Buffer;
        typedef TxOnewire *TxOnewirePntr;

        const TxOnewirePntr _tx_onewire;
        Buffer buffer;

    public:
        BufferedTxOnewire(TxOnewire *tx_onewire) : _tx_onewire(tx_onewire)
        {
        }
        bool transmitted() const
        {
            return buffer.is_empty() && _tx_onewire->transmitted();
        }

        void disable_async()
        {
            _tx_onewire->async = false;
        }

        void setup()
        {
#ifdef DOLOG
            ESP_LOGI(TAG, "receive: using buffered tx");
#endif
            _tx_onewire->setup();
        }
        void transmit(Value value)
        {
            buffer.push(value);
        }

        void loop(Micros now)
        {
            _tx_onewire->loop(now);
            if (!buffer.is_empty() && _tx_onewire->transmitted())
            {
                _tx_onewire->transmit(buffer.pop());
                if (!buffer.is_empty())
                {
#ifdef DOLOG
                    ESP_LOGW(TAG, "receive: buffer not empty, size: %d", buffer.size());
#endif
                }
            }
        }
    };

}

class OneWireTracker : public onewire::Rx, public onewire::Tx
{
    bool last = false;
    bool changed = true;

public:
    bool pending()
    {
        if (changed)
            return true;

        auto state = read();
        if (last == state)
            return false;

        last = state;
        changed = true;
        return true;
    }

    bool replicate()
    {
        write(last);
        changed = false;
        return last;
    }
};

class OneWireProtocol : public onewire::RxOnewire, public onewire::TxOnewire
{
public:
private:
public:
    void loop();

    void setup()
    {
        Rx::setup();
        Tx::setup();
    }

    void listen();
    int delay() const { return onewire::_tx_delay; }
};

#endif
