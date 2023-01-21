#pragma once

#include "../clocks_shared/log.h"
#include "../clocks_shared/onewire.h"

namespace onewire {
class Tx {
#ifdef DOLED
  int last_written = -1;
#endif

 public:
  void setup() {
    pinMode(SYNC_OUT_PIN, OUTPUT);
    write(false);
  }

  MOVE2RAM void write(bool state) __attribute__((always_inline)) {
    PIN_WRITE(SYNC_OUT_PIN, state);
#ifdef DOLED
    if ((state ? 1 : 0) != last_written) {
      last_written = state ? 1 : 0;
      Leds::set_ex(LED_SYNC_OUT, state ? LedColors::black : LedColors::purple);
    }
#endif
  }
};

class RawTxOnewire : public Tx {
 private:
  int tx_pulses;
  Micros tx_start;

 protected:
  bool started = false;

  int hi_length = 0;
  int bit_state = false;

  const int8_t LAST_TX_BIT = MAX_DATA_BITS + 2;
  int8_t _tx_bit = LAST_TX_BIT;

  onewire::Value _tx_value, _tx_remainder_value, _tx_transmitted_value;

 public:
  RawTxOnewire() {}

  void dump_config() {
    LOGI(TAG, "  TxOnewire");
    LOGI(TAG, "     re_pin: %d", RS485_RE_PIN);
    LOGI(TAG, "     OnewireInterrupt::tx: 0x%d", OnewireInterrupt::tx);
    // LOGI(TAG, "     state: %d", state);
  }

  MOVE2RAM void timer_interrupt() {
    auto bit = this->_tx_bit;
    if (bit == MAX_DATA_BITS + 2) {
      return;
    }

    if (bit == MAX_DATA_BITS + 1) {
      write(false);

      ESP_LOGD(
          TAG,
          "transmit: DONE %d, total_time=%d pulses=%d(?=%d) pulse_length=%d "
          "expected=%d",
          _tx_value, micros() - tx_start, tx_pulses,
          3 + MAX_DATA_BITS + __builtin_popcountl(_tx_value) + 1,
          (micros() - tx_start) / tx_pulses, int(1000000 / USED_BAUD));
      this->_tx_bit++;
      return;
    }

    if (bit >= 0 && this->hi_length) {
      tx_pulses++;
      ESP_LOGV(TAG, "transmit: PULSE=%s (%d) INTER",
               this->bit_state ? "HI" : "LOW", tx_pulses);
      this->hi_length--;
      return;
    }

    if (bit == MAX_DATA_BITS) {
      this->bit_state = !this->bit_state;
      write(this->bit_state);
      this->_tx_bit++;
      this->tx_pulses++;
      ESP_LOGV(TAG, "transmit: PULSE=%s (%d) END",
               this->bit_state ? "HI" : "LOW", tx_pulses);
      return;
    }

    if (bit >= 0 && bit < onewire::MAX_DATA_BITS) {
      auto value = this->_tx_remainder_value & 1;
      this->_tx_remainder_value >>= 1;
      this->hi_length = value ? 1 : 0;
      this->tx_pulses++;
      this->bit_state = !this->bit_state;
      write(this->bit_state);
      ESP_LOGV(TAG, "transmit: bit=%d bit_value=%d (data)", bit, value ? 1 : 0);
      ESP_LOGV(TAG, "transmit: PULSE=%s (%d) DATA",
               this->bit_state ? "HI" : "LOW", tx_pulses);
      this->_tx_bit++;
      return;
    } else if (bit == -1) {
      ESP_LOGD(TAG, "transmit: START value=%d", _tx_value);

      tx_start = micros();
      tx_pulses = 1;

      this->_tx_remainder_value = _tx_value;
      this->hi_length = 2;

      this->bit_state = true;
      write(true);
      ESP_LOGV(TAG, "transmit: PULSE=HI (1) START");

      this->_tx_bit = 0;
      return;
    } else {
      ESP_LOGW(TAG, "CODE TO BE ADDED! bit=%d other", bit);
      return;
    }
  }

  void kill() {
    ESP_LOGW(TAG, "kill()");
    OnewireInterrupt::kill();
  }

  void setup() {
    Tx::setup();

    OnewireInterrupt::attach();
  }

  void begin() { started = true; }

  bool active() const { return started; }

  void transmit(onewire::Value value) {
    onewire::Value masked_value = value & DATA_MASK;
    if (masked_value != value) {
      ESP_LOGW(TAG, "Too many bits, data will be lost!? value=%d, max_bits=%d",
               value, MAX_DATA_BITS);
    }

    // OnewireInterrupt::disableTimer();
    if (_tx_bit != MAX_DATA_BITS + 2) {
      ESP_LOGW(TAG, "transmit not complete !?: tx_value=%d, tx_bit=%d)",
               _tx_value, _tx_bit);
    }
    _tx_bit = LAST_TX_BIT;

    _tx_value = masked_value;
    _tx_remainder_value = _tx_value;
    _tx_transmitted_value = 0;

    // this one should be done last
    _tx_bit = -1;
    if (_tx_value != value) {
      ESP_LOGW(TAG, "Warning masked value(=%d) is not same as input(=%d)",
               _tx_value, value);
    }
    ESP_LOGD(TAG, "transmit: START tx_value=%d", _tx_value);
    ESP_LOGVV(TAG, "transmit: START HIGH tx_value=%d, tx_bit=%d)", _tx_value,
              _tx_bit);
    // OnewireInterrupt::enableTimer();
  }

  MOVE2RAM bool transmitted() const {
    return started && (_tx_bit == MAX_DATA_BITS + 2);
  }
};

class TxOnewire {
 private:
  volatile bool locked = false;
  RawTxOnewire _raw_tx;
  RingBuffer<onewire::Value, ONEWIRE_BUFFER_SIZE> buffer;

 public:
  TxOnewire() {}
  void dump_config() { _raw_tx.dump_config(); }

  void kill() { _raw_tx.kill(); }

  bool transmitted() const {
    return buffer.is_empty() && _raw_tx.transmitted();
  }

  void setup(int _buffer_size = -1) {
    buffer.setup(_buffer_size);
    LOGI(TAG, "receive: using buffered tx");
    _raw_tx.setup();
    OnewireInterrupt::tx = this;
  }

  void transmit(Value value) {
    while (locked) {
      yield();
    }
    locked = true;
    buffer.push(value);
    if (onewire::one_wire_double_check) buffer.push(value);
    locked = false;
  }

  void begin() { _raw_tx.begin(); }

  bool active() const __attribute__((always_inline)) {
    return _raw_tx.active();
  }

  MOVE2RAM void timer_interrupt() __attribute__((always_inline)) {
    locked = true;
    if (!buffer.is_empty() && _raw_tx.transmitted()) {
      _raw_tx.transmit(buffer.pop());
      if (!buffer.is_empty()) {
        ESP_LOGD(TAG, "tx: buffer not empty, size: %d", buffer.size());
      }
      locked = false;
      return;
    } else {
      locked = false;
      _raw_tx.timer_interrupt();
    }
  }
};
}  // namespace onewire