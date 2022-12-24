#ifndef ONEWIRE_H
#define ONEWIRE_H

// HHL = START
// HL = 1
// LH = 0
// LHL = END

#include "pinio.h"
#include "pins.h"
#include "ringbuffer.h"

#ifdef SLAVE
#define DOLED
#endif

#ifdef DOLED
#include "slave/leds.h"
#endif

constexpr int8_t MAX_DATA_BITS = 32;

#ifdef ESP8266

// #define USE_RX_INTERRUPT

#define MOVE2RAM IRAM_ATTR

// credits to: https://github.com/PaulStoffregen/Encoder/
#define IO_REG_TYPE uint32_t
#define PIN_TO_BASEREG(pin) ((volatile uint32_t *)(0x60000000 + (0x318)))
#define PIN_TO_BITMASK(pin) (digitalPinToBitMask(pin))
#define DIRECT_PIN_READ(base, mask) (((*(base)) & (mask)) ? 1 : 0)
// deducted from DIRECT_PIN_READ
#define DIRECT_PIN_SET(base, mask) *(base) |= mask
#define DIRECT_PIN_CLEAR(base, mask) *(base) -= *(base)&mask

#else
#define MOVE2RAM
#endif

namespace onewire
{
    constexpr uint32_t RX_BAUD = 1500;
    constexpr uint32_t TX_BAUD = RX_BAUD;

    class RxOnewire;
    class TxOnewire;

    // for now globally fixed
    // constexpr uint32_t RX_BAUD = 100;
    // constexpr uint32_t TX_BAUD = RX_BAUD;
    constexpr int8_t START_BITS = 2;

    typedef uint16_t Value;
    const Value DATA_MASK = ~Value(0);

#ifdef ESP8266
    static volatile uint32_t *rx_basereg = PIN_TO_BASEREG(SYNC_IN_PIN);
    static IO_REG_TYPE rx_bitmask = PIN_TO_BITMASK(SYNC_IN_PIN);

    static volatile uint32_t *tx_basereg = PIN_TO_BASEREG(SYNC_OUT_PIN);
    static IO_REG_TYPE tx_bitmask = PIN_TO_BITMASK(SYNC_OUT_PIN);
#endif

    class OnewireInterrupt
    {

    public:
        static int timer_attach_state;
        static RxOnewire *rx;
        static TxOnewire *tx;

        static void attach();
        static void kill();

        // to be refactored
        static void disableTimer();
        static void enableTimer();
    };

    class Rx
    {
        bool _rx_last_state = false, _rx_state = false;

#ifdef DOLED
        int last_read = -1;
#endif
    protected:
        bool last_state() const
        {
            return _rx_last_state;
        }

        MOVE2RAM bool read() __attribute__((always_inline))
        {
            _rx_last_state = _rx_state;
            _rx_state = PIN_READ(SYNC_IN_PIN);

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
        void setup()
        {
            pinMode(SYNC_OUT_PIN, OUTPUT);
            write(false);
        }

        MOVE2RAM void write(bool state) __attribute__((always_inline))
        {
            PIN_WRITE(SYNC_OUT_PIN, state);
#ifdef DOLED
            if ((state ? 1 : 0) != last_written)
            {
                last_written = state ? 1 : 0;
                Leds::set(2, state ? rgb_color(0xFF, 0, 0) : rgb_color(0x00, 0xFF, 0));
            }
#endif
        }
    };

    const int8_t RX_BIT_INITIAL = -onewire::START_BITS - 1;

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
        // last read value, valid if _rx_available is true
        volatile onewire::Value _rx_last_value;
        volatile bool _rx_available = false;

        Micros _rx_tstart;
        uint32_t _rx_delay = 0;

        // vars for the scanner
        volatile int8_t _rx_bit = RX_BIT_INITIAL;
        volatile bool _rx_nibble = false;     //
        volatile onewire::Value _rx_value;    // keep track of the scanned data
        volatile bool _rx_last_state = false; // last read state

        void reset(bool forced);
        void reset_interrupt()
        {
            _rx_nibble = false;
            _rx_bit = onewire::RX_BIT_INITIAL;
        }

    public:
        MOVE2RAM void timer_interrupt();
        MOVE2RAM void timer_interrupt(bool last, bool current);

        MOVE2RAM void handle_interrupt(bool rising);
        /***
         * pending:
         * true: a byte is available
         * false: no byte is available
         */
        bool pending() const
        {
            return _rx_available;
        }

        void kill()
        {
        }

        /***
         * if pending() == true, then returns the read byte, otherwise -1
         */
        onewire::Value flush()
        {
            _rx_available = false;
            return _rx_last_value;
        }
        void begin(int baud);
    };

    class TxOnewire : public Tx
    {
    protected:
        Micros _tx_tstart;
        uint32_t _tx_delay;

        const int8_t LAST_TX_BIT = MAX_DATA_BITS + 4;
        int8_t _tx_bit = LAST_TX_BIT;
        bool _tx_nibble = false;

        onewire::Value _tx_value, _tx_remainder_value;

        void write_to_sync();

    public:
        TxOnewire(int baud) : _tx_delay(1000000L / baud) {}

        void dump_config();

        void kill();

        void setup();

        void transmit(onewire::Value value);

        MOVE2RAM void timer_interrupt();

        bool transmitted() const
        {
            return _tx_delay == 0 || _tx_bit == MAX_DATA_BITS + 4;
        }
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
        void dump_config()
        {
            _tx_onewire->dump_config();
        }

        void kill()
        {
            _tx_onewire->kill();
        }

        bool transmitted() const
        {
            return buffer.is_empty() && _tx_onewire->transmitted();
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

        MOVE2RAM void loop(Micros now)
        {
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

#endif
