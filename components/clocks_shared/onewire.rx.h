#pragma once

#include "../clocks_shared/log.h"
#include "../clocks_shared/onewire.h"

namespace onewire {
class Rx {
  bool _rx_state = false;

#ifdef DOLED
  int last_read = -1;
#endif

 public:
  void setup() { pinMode(SYNC_IN_PIN, INPUT); }
};

constexpr Micros bit_block = 1000000L / USED_BAUD;
constexpr Micros start_block = bit_block * 2.8;
constexpr Micros one_block = bit_block * 1.8;

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
 private:
  Micros rx_start = 0;
  Value _rx_value = 0;

 protected:
  RingBuffer<onewire::Value, ONEWIRE_BUFFER_SIZE> buffer;
  void debug(onewire::Value value);

  // vars for the scanner, _rx_bit is shared with main!
  volatile int8_t _rx_bit = RX_BIT_INITIAL;
  bool _rx_last_state = false;  // last read state

 public:
  int rx_start_detected_after_data = 0;
  int rx_too_long_empty_detected_after_data = 0;

  void reset_error_count() {
    rx_start_detected_after_data = rx_too_long_empty_detected_after_data = 0;
  }
  int error_count() const {
    return rx_start_detected_after_data + rx_too_long_empty_detected_after_data;
  }

  MOVE2RAM void follow_change() {
    if (this->_rx_bit >= 0 && this->_rx_bit < MAX_DATA_BITS) {
      Micros delta = micros() - rx_start;
      this->rx_start = micros();

      if (delta > start_block) {
        auto current_state = PIN_READ(SYNC_IN_PIN);
        if (current_state) {
          LOGV(TAG,
               "receive:  START DETECTED !? while reading data !! delta=%d "
               "start_block=%d",
               delta, start_block);
          rx_start_detected_after_data++;

          this->_rx_value = 0;
          this->_rx_bit = 0;
          this->rx_start = micros();
        } else {
          LOGV(TAG,
               "receive:  TO LONG NO DATA ON THE LINE !? !? while reading data "
               "!! delta=%d "
               "start_block=%d",
               delta, start_block);
          rx_too_long_empty_detected_after_data++;
          this->_rx_bit = RX_BIT_INITIAL;
        }
        return;
      }
      bool state = delta > one_block;
      if (state) {
        this->_rx_value |= Value(1) << _rx_bit;
      }
      LOGV(TAG, "receive: bit=%d READ: %d value=%d", this->_rx_bit,
           state ? 0 : 1, this->_rx_value);

      this->_rx_bit++;
      if (this->_rx_bit == MAX_DATA_BITS) {
        this->_rx_bit = RX_BIT_INITIAL;
        LOGV(TAG, "receive:  GOT %d", this->_rx_value);
        buffer.push(this->_rx_value);
      }
      return;
    }

    if (this->_rx_bit == RX_BIT_INITIAL + 1) {
      Micros delta = micros() - rx_start;
      if (delta > start_block) {
        LOGV(TAG, "receive: START DETECTED");
        this->_rx_value = 0;
        this->_rx_bit = 0;
        this->rx_start = micros();
      } else {
        LOGV(TAG, "START too short, expected at least %d got %d (RESET)",
             start_block, delta);
        this->_rx_bit = RX_BIT_INITIAL;
      }
      return;
    }

    if (this->_rx_bit == RX_BIT_INITIAL) {
      auto current_state = PIN_READ(SYNC_IN_PIN);
      LOGVV(TAG, "Scan for start... current_state=%s",
            current_state ? "HI" : "LOW");
      if (current_state == true) {
        this->rx_start = micros();
        this->_rx_bit = RX_BIT_INITIAL + 1;
      }
      return;
    }

    LOGW(TAG, "TO BE CONTINUE");
  }

  /***
   * pending:
   * true: a byte is available
   * false: no byte is available
   */
  bool pending() const {
    DISABLE_TIMER_INTERUPT();
    auto ret = !buffer.is_empty();
    ENABLE_TIMER_INTERUPT();
    return ret;
  }

  bool reading() const { return _rx_bit != RX_BIT_INITIAL; }

  void kill() {}

  /***
   * if pending() == true, then returns the read byte, otherwise -1
   */
  onewire::Value flush() {
    DISABLE_TIMER_INTERUPT();
    auto val = buffer.is_empty() ? 0 : buffer.pop();
    ENABLE_TIMER_INTERUPT();
    return val;
  }

  void begin() {
    OnewireInterrupt::attach();
    OnewireInterrupt::rx = this;
    LOGI(TAG, "OneWireProtocol: %fbaud", (float)USED_BAUD);
  }
};
}  // namespace onewire