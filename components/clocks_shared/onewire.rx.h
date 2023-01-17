#pragma once

#define USE_RX_BUFFER

#include "../clocks_shared/log.h"
#include "../clocks_shared/onewire.h"

namespace onewire {
class Rx {
  bool _rx_last_state = false, _rx_state = false;

#ifdef DOLED
  int last_read = -1;
#endif
 protected:
  bool last_state() const { return _rx_last_state; }

  MOVE2RAM bool read() __attribute__((always_inline)) {
    _rx_last_state = _rx_state;
    _rx_state = PIN_READ(SYNC_IN_PIN);

#ifdef DOLED
    if ((_rx_state ? 1 : 0) != last_read) {
      last_read = _rx_state ? 1 : 0;
    }
#endif
    return _rx_state;
  }

 public:
  void setup() { pinMode(SYNC_IN_PIN, INPUT); }
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
class RxOnewire : public onewire::Rx {
 protected:
  friend class OnewireInterrupt;

#ifdef USE_RX_BUFFER
  RingBuffer<onewire::Value, ONEWIRE_BUFFER_SIZE> buffer;
  void debug(onewire::Value value);
#else
  // last read value, valid if _rx_available is true
  volatile onewire::Value _rx_last_value;
  volatile bool _rx_available = false;
#endif

  // vars for the scanner
  volatile int8_t _rx_bit = RX_BIT_INITIAL;
  volatile bool _rx_nibble = false;      //
  volatile onewire::Value _rx_value;     // keep track of the scanned data
  volatile bool _rx_last_state = false;  // last read state

  void reset(bool forced) {
    if (forced && _rx_bit != RX_BIT_INITIAL) {
#ifdef DOLED
      Leds::set_ex(LED_ONEWIRE, LedColors::red);
#endif
      ESP_LOGW(TAG, "receive: RESET rx_value=%d, rx_bit=%d) -> pre start bit",
               _rx_value, _rx_bit);
    } else {
#ifdef DOLED
      //    Leds::set_ex(LED_ONEWIRE, LedColors::blue);
#endif
    }
    _rx_value = 0;
    _rx_bit = RX_BIT_INITIAL;
    _rx_nibble = false;
  }
  void reset_interrupt() {
    _rx_nibble = false;
    _rx_bit = onewire::RX_BIT_INITIAL;
  }

 public:
  MOVE2RAM void timer_interrupt() {
    auto current = PIN_READ(SYNC_IN_PIN);
    timer_interrupt(_rx_last_state, current);
    _rx_last_state = current;
  }

  MOVE2RAM void timer_interrupt(bool last, bool current) {
    if (_rx_bit >= 0 && _rx_bit < MAX_DATA_BITS) {
      if (_rx_nibble) {
        _rx_nibble = false;
        if (!current)  // databit
          _rx_bit++;
        else if (current && !last) {  // actual end symbol
          _rx_bit = MAX_DATA_BITS + 2;
          ESP_LOGVV(TAG, "receive:  Possible END");
        } else {
          // invalid state!
          ESP_LOGW(TAG, "receive:  INVALID !?");
          reset_interrupt();
        }
        return;
      } else {
        if (current) _rx_value |= onewire::Value(1) << onewire::Value(_rx_bit);
        ESP_LOGVV(TAG, "receive:  DATAF: %s @%d (%d)", current ? "HIGH" : "LOW",
                  _rx_bit, _rx_value);
        _rx_nibble = true;
        return;
      }
    }

    switch (_rx_bit) {
      case MAX_DATA_BITS + 2:
        if (current) {
          ESP_LOGW(TAG, "receive:  INVALID END !?");
        } else {
          ESP_LOGD(TAG, "receive:  END -> %d", _rx_value);
          debug(_rx_value);

#ifdef USE_RX_BUFFER
          if (!buffer.is_empty()) {
            ESP_LOGW(TAG,
                     "receive:  Previous recieved value not processed, will "
                     "be buffered by %d (size=%d",
                     _rx_value, buffer.size());
          }
          buffer.push(_rx_value);
#else
          if (_rx_available) {
            ESP_LOGW(TAG,
                     "receive:  Previous recieved value %d not processed, will "
                     "be overwritten by %d",
                     _rx_last_value, _rx_value);
          }
          _rx_last_value = _rx_value;
          _rx_available = true;
#endif
#ifdef DOLED
          Leds::set_ex(LED_ONEWIRE, LedColors::green);
#endif
        }
        reset_interrupt();
        return;

      case MAX_DATA_BITS + 1:
      case MAX_DATA_BITS:
        if (current != (_rx_bit == MAX_DATA_BITS ? false : true)) {
          LOGI(TAG, "NOT EXPECTED !?");
          reset_interrupt();
        } else
          _rx_bit++;
        return;

      case onewire::RX_BIT_INITIAL:
        if (_rx_nibble) {
          if (current) {
            // snipz... reset
            ESP_LOGVV(TAG, "RESET!");
            reset_interrupt();
            return;
          }
          ESP_LOGVV(TAG, "receive:  START DETECTED");
          _rx_bit = 0;
          _rx_nibble = false;
          _rx_value = 0;
#ifdef DOLED
          Leds::set_ex(LED_ONEWIRE, LedColors::blue);
#endif
          return;
        } else if (current && last)
          _rx_nibble = true;
        return;

      default:
        ESP_LOGVV(TAG, "receive:  UNKNOWN STATE!?");
        reset_interrupt();
    }
  }

  MOVE2RAM void handle_interrupt(bool rising);
  /***
   * pending:
   * true: a byte is available
   * false: no byte is available
   */
  bool pending() const {
#ifdef USE_RX_BUFFER
    return !buffer.is_empty();
#else
    return _rx_available;
#endif
  }

  bool reading() const { return _rx_bit != RX_BIT_INITIAL; }

  void kill() {}

  /***
   * if pending() == true, then returns the read byte, otherwise -1
   */
  onewire::Value flush() {
#ifdef USE_RX_BUFFER
    return buffer.pop();
#else
    _rx_available = false;
    return _rx_last_value;
#endif
  }
  void begin() {
    OnewireInterrupt::attach();
    OnewireInterrupt::rx = this;

    reset(false);
    LOGI(TAG, "OneWireProtocol: %fbaud", (float)USED_BAUD);
  }
};
}  // namespace onewire