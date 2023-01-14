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
 protected:
  bool started = false;

  const int8_t LAST_TX_BIT = MAX_DATA_BITS + 4;
  int8_t _tx_bit = LAST_TX_BIT;
  bool _tx_nibble = false;

  onewire::Value _tx_value, _tx_remainder_value, _tx_transmitted_value;

  void write_to_sync() {
    if (_tx_bit == -onewire::START_BITS - 1) {
      _tx_bit++;
      write(true);
      return;
    }

    if (_tx_bit < 0) {
      bool bit = _tx_bit != -1;
      write(bit);
      ESP_LOGVV(TAG, "transmit: START %s tx_value=%d, tx_bit=%d)",
                bit ? "HIGH" : "LOW ", _tx_value, _tx_bit);
      _tx_bit++;
      return;
    }

    if (_tx_bit < MAX_DATA_BITS) {
      onewire::Value mask = onewire::Value(1) << onewire::Value(_tx_bit);
      bool bit = _tx_remainder_value & mask;
      // if (bit && !_tx_nibble)
      bool written = bit && !_tx_nibble;
      write(written);
      if (written) {
        _tx_transmitted_value |= mask;
        _tx_remainder_value -= mask;
      }
      ESP_LOGVV(
          TAG,
          "transmit: DATA%s %s tx_value=%d, _tx_transmitted=%d, tx_bit=%d)",
          _tx_nibble ? "S" : "F", written ? "HIGH" : "LOW ", _tx_value,
          _tx_transmitted_value, _tx_bit);
      if (_tx_nibble) {
        _tx_bit = _tx_remainder_value == 0 ? MAX_DATA_BITS : _tx_bit + 1;
        //_tx_bit++;
      }
      _tx_nibble = !_tx_nibble;
      return;
    }

    if (_tx_bit == MAX_DATA_BITS) {
      ESP_LOGVV(TAG, "transmit: END1  LOW  tx_value=%d, tx_bit=%d)", _tx_value,
                _tx_bit);

      write(false);
      _tx_bit++;
    } else if (_tx_bit == MAX_DATA_BITS + 1) {
      ESP_LOGVV(TAG, "transmit: END2  HIGH tx_value=%d, tx_bit=%d)", _tx_value,
                _tx_bit);
      write(true);

      _tx_bit++;
    } else if (_tx_bit == MAX_DATA_BITS + 2) {
      ESP_LOGVV(TAG, "transmit: END3  LOW tx_value=%d, tx_bit=%d)", _tx_value,
                _tx_bit);
      write(false);

      _tx_bit++;
    } else if (_tx_bit == MAX_DATA_BITS + 3) {
      ESP_LOGVV(TAG, "transmit: FINAL tx_value=%d, tx_bit=%d)", _tx_value,
                _tx_bit);
      ESP_LOGD(TAG, "transmit: END tx_value=%d", _tx_value);
      _tx_bit = LAST_TX_BIT;
    }
  }

 public:
  RawTxOnewire() {}

  void dump_config() {
    LOGI(TAG, "  TxOnewire");
    LOGI(TAG, "     re_pin: %d", RS485_RE_PIN);
    LOGI(TAG, "     OnewireInterrupt::tx: 0x%d", OnewireInterrupt::tx);
    // LOGI(TAG, "     state: %d", state);
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
    if (_tx_bit != LAST_TX_BIT && _tx_bit != LAST_TX_BIT + 1) {
      ESP_LOGW(TAG, "transmit not complete !?: tx_value=%d, tx_bit=%d)",
               _tx_value, _tx_bit);
    }
    _tx_bit = LAST_TX_BIT;

    _tx_nibble = false;
    _tx_value = masked_value;
    _tx_remainder_value = _tx_value;
    _tx_transmitted_value = 0;

    // this one should be done last
    _tx_bit = -onewire::START_BITS - 1;
    if (_tx_value != value) {
      ESP_LOGW(TAG, "Warning masked value(=%d) is not same as input(=%d)",
               _tx_value, value);
    }
    ESP_LOGD(TAG, "transmit: START tx_value=%d", _tx_value);
    ESP_LOGVV(TAG, "transmit: START HIGH tx_value=%d, tx_bit=%d)", _tx_value,
              _tx_bit);
    // OnewireInterrupt::enableTimer();
  }

  MOVE2RAM void timer_interrupt() {
    if (_tx_bit == LAST_TX_BIT + 4) {
      // OnewireInterrupt::disableTimer();
      // done
      return;
    }
    write_to_sync();
    return;
  }

  MOVE2RAM bool transmitted() const {
    return started && (_tx_bit == LAST_TX_BIT);
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
        ESP_LOGW(TAG, "receive: buffer not empty, size: %d", buffer.size());
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