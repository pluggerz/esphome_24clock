#pragma once

#include "../clocks_shared/log.h"
#include "../clocks_shared/pinio.h"
#include "../clocks_shared/pins.h"
#include "../clocks_shared/stub.h"

namespace rs485 {
const char *const TAG = "channel";

// Helper class to calucalte CRC8
class CRC8 {
 public:
  static byte calc(const byte *addr, byte len) {
    byte crc = 0;
    while (len--) {
      byte inbyte = *addr++;
      for (byte i = 8; i; i--) {
        byte mix = (crc ^ inbyte) & 0x01;
        crc >>= 1;
        if (mix) crc ^= 0x8C;
        inbyte >>= 1;
      }  // end of for
    }    // end of while
    return crc;
  }
};

class Gate  // : public Gate
{
  enum State { RECEIVING = 1, TRANSMITTING = 2, NONE = 0 } state = NONE;

 public:
  void dump_config() {
    LOGI(TAG, "  rs485::Gate");
    LOGI(TAG, "     de_pin: %d", RS485_DE_PIN);
    LOGI(TAG, "     re_pin: %d", RS485_RE_PIN);
    LOGI(TAG, "     state: %d", state);
  }

  bool is_receiving() const { return state == RECEIVING; }

  void setup() {
    pinMode(RS485_DE_PIN, OUTPUT);
    pinMode(RS485_RE_PIN, OUTPUT);
#ifdef DOLED
    // Leds::set_ex(LED_CHANNEL_STATE, LedColors::green);
#endif
    LOGI(TAG, "state: setup");
  }

  void start_receiving() {
    if (state == RECEIVING) return;

    // make sure we are done with sending
    Serial.flush();

    PIN_WRITE(RS485_DE_PIN, false);
    PIN_WRITE(RS485_RE_PIN, false);
#ifdef DOLED
    Leds::set_ex(LED_CHANNEL_STATE, LedColors::blue);
#endif

    state = RECEIVING;
    LOGI(TAG, "state: RECEIVING");
  }
  void start_transmitting() {
    if (state == TRANSMITTING) return;

    PIN_WRITE(RS485_DE_PIN, HIGH);
    PIN_WRITE(RS485_RE_PIN, LOW);

    state = TRANSMITTING;
    LOGI(TAG, "state: TRANSMITTING");
  }
};

template <uint16_t SIZE>
class Buffer {
 private:
  byte bytes[SIZE];
  uint8_t size_;

 public:
  uint8_t size() const { return SIZE; }
  byte *raw() { return bytes; }
};

class Protocol {
 public:
  virtual void set_buffer(byte *data, const int length) = 0;
  virtual void reset(const char *reason) = 0;
  virtual void sendMsg(const byte *data, const byte length) = 0;

  static Protocol *create_default();
};

class Channel {
 protected:
  typedef uint32_t Baudrate;
  Baudrate _baudrate = 0;
  Protocol *_protocol = Protocol::create_default();
  Gate gate;

  void _send(const byte *bytes, const byte length);

  template <class M>
  void _send(const M &m) {
    _send((const byte *)&m, (byte)sizeof(M));
  }

 public:
  bool bytes_available_for_write(int bytes) const;

  void start_receiving() {
    gate.start_receiving();
    if (_protocol) _protocol->reset("start_receiving");
  }

  // NOTE: 'start_transmitting' should be private
  void start_transmitting() {
    gate.start_transmitting();
    if (_protocol) _protocol->reset("start_transmitting");
  }

  void dump_config() { gate.dump_config(); }

  Baudrate baudrate() { return _baudrate; }

  void baudrate(Baudrate baud_rate) {
    if (_baudrate) Serial.end();
    _baudrate = baud_rate;
    Serial.begin(_baudrate);

    LOGI(TAG, "state: Serial.begin(%d)", baud_rate);
  }

  void setup() { gate.setup(); }
  void loop();

  virtual void process(const byte *bytes, const byte length) = 0;
};

constexpr int RECEIVER_BUFFER_SIZE = 128;

class BufferChannel : public Channel {
 private:
  Buffer<RECEIVER_BUFFER_SIZE> buffer;

 public:
  void setup() {
    _protocol->set_buffer(buffer.raw(), RECEIVER_BUFFER_SIZE);

    Channel::setup();
  }

  void skip();

  template <class M>
  void send(const M &m) {
    _send(m);
  }
  void raw_send(const byte *bytes, const byte length) { _send(bytes, length); }
};
}  // namespace rs485