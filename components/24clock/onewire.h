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
    constexpr uint32_t BAUD = 500;

    class RxOnewire;
    class TxOnewire;

    constexpr int8_t START_BITS = 2;

    typedef uint32_t Value;
    constexpr int8_t MAX_DATA_BITS = sizeof(Value) * 8;
    constexpr Value DATA_MASK = Value((uint64_t(1) << uint64_t(MAX_DATA_BITS)) - 1);

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
        static void restart();
        static void kill();
        static void align();

        static void dump_config();

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
                Leds::set_ex(LED_SYNC_OUT, state ? LedColors::black : LedColors::purple);
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

        bool reading() const
        {
            return _rx_bit != RX_BIT_INITIAL;
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
        void begin();
    };

    class RawTxOnewire : public Tx
    {
    protected:
        bool started = false;

        uint32_t _tx_delay;

        const int8_t LAST_TX_BIT = MAX_DATA_BITS + 4;
        int8_t _tx_bit = LAST_TX_BIT;
        bool _tx_nibble = false;

        onewire::Value _tx_value, _tx_remainder_value, _tx_transmitted_value;

        void write_to_sync();

    public:
        RawTxOnewire() : _tx_delay(1000000L / BAUD) {}

        void dump_config();

        void kill();

        void setup();

        void begin()
        {
            started = true;
        }

        bool active() const
        {
            return started;
        }

        void transmit(onewire::Value value);

        MOVE2RAM void timer_interrupt();

        MOVE2RAM bool transmitted() const
        {
            return started && (_tx_delay == 0 || _tx_bit == LAST_TX_BIT);
        }
    };

    constexpr int SIZE = 8;

    class TxOnewire
    {
    private:
        RawTxOnewire _raw_tx;
        RingBuffer<onewire::Value, SIZE> buffer;

    public:
        TxOnewire()
        {
        }
        void dump_config()
        {
            _raw_tx.dump_config();
        }

        void kill()
        {
            _raw_tx.kill();
        }

        bool transmitted() const
        {
            return buffer.is_empty() && _raw_tx.transmitted();
        }

        void setup(int _buffer_size = -1)
        {
            buffer.setup(_buffer_size);
#ifdef DOLOG
            ESP_LOGI(TAG, "receive: using buffered tx");
#endif
            _raw_tx.setup();
            OnewireInterrupt::tx = this;
        }

        void transmit(Value value)
        {
            buffer.push(value);
        }

        void begin()
        {
            _raw_tx.begin();
        }

        bool active() const __attribute__((always_inline))
        {
            return _raw_tx.active();
        }

        MOVE2RAM void timer_interrupt() __attribute__((always_inline))
        {
            if (!buffer.is_empty() && _raw_tx.transmitted())
            {
                _raw_tx.transmit(buffer.pop());
                if (!buffer.is_empty())
                {
#ifdef DOLOG
                    ESP_LOGW(TAG, "receive: buffer not empty, size: %d", buffer.size());
#endif
                }
                return;
            }
            _raw_tx.timer_interrupt();
        }
    };
}

#endif
