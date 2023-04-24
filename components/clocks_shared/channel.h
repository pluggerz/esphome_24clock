#pragma once

#include "../clocks_shared/log.h"
#include "../clocks_shared/pinio.h"
#include "../clocks_shared/pins.h"
#include "../clocks_shared/stub.h"

#ifdef ESP8266
#define SerialDelegate Serial1
#else
#define SerialDelegate Serial
#endif

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
#ifdef MASTER
    LOGI(TAG, "     re_pin: N/A");
#else
    LOGI(TAG, "     re_pin: %d", RS485_RE_PIN);
#endif
    LOGI(TAG, "     state: %d", state);
  }

  bool is_receiving() const { return state == RECEIVING; }

  void setup() {
    pinMode(RS485_DE_PIN, OUTPUT);
#ifndef MASTER
    pinMode(RS485_RE_PIN, OUTPUT);
#endif
#ifdef DOLED
    // Leds::set_ex(LED_CHANNEL_STATE, LedColors::green);
#endif
    LOGI(TAG, "state: setup");
  }

#ifndef MASTER
  void start_receiving() {
    if (state == RECEIVING) return;

    // make sure we are done with sending
    SerialDelegate.flush();

    PIN_WRITE(RS485_DE_PIN, false);
    PIN_WRITE(RS485_RE_PIN, false);

#ifdef DOLED
    Leds::set_ex(LED_CHANNEL_STATE, LedColors::blue);
#endif

    state = RECEIVING;
    LOGI(TAG, "state: RECEIVING");
  }
#endif

#ifdef MASTER
  void start_transmitting() {
    if (state == TRANSMITTING) return;

    PIN_WRITE(RS485_DE_PIN, HIGH);
#ifndef MASTER
    PIN_WRITE(RS485_RE_PIN, LOW);
#endif
    state = TRANSMITTING;
    LOGI(TAG, "state: TRANSMITTING");
  }
#endif
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
  unsigned long skipped_message_count;

  virtual void set_buffer(byte *data, const int length) = 0;
  virtual void sendMsg(const byte *data, const byte length) = 0;
  virtual void start_receiving() {}
  virtual void start_transmitting() {}
  virtual int errors() const;

  bool is_skippable_message(byte first_byte);

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

#ifndef MASTER
  void start_receiving() {
    gate.start_receiving();
    if (_protocol) _protocol->start_receiving();
  }
#endif

#ifdef MASTER
  // NOTE: 'start_transmitting' should be private
  void start_transmitting() {
    gate.start_transmitting();
    if (_protocol) _protocol->start_transmitting();
  }
#endif

  void dump_config() {
    LOGI(TAG, "channel: %d BAUD", _baudrate);
    gate.dump_config();
  }

  Baudrate baudrate() { return _baudrate; }

  void begin(Baudrate baud_rate) {
    spit_info('^', baud_rate / 100);

    if (_baudrate) SerialDelegate.end();
    _baudrate = baud_rate;
    SerialDelegate.begin(_baudrate);

    LOGI(TAG, "state: SerialDelegate.begin(%d)", baud_rate);
  }

  void setup() { gate.setup(); }
  void loop();

  virtual void process(const byte *bytes, const byte length) = 0;
};

constexpr int RECEIVER_BUFFER_SIZE = 100;

class BufferChannel : public Channel {
 private:
  Buffer<RECEIVER_BUFFER_SIZE> buffer;

 public:
  void setup() {
    _protocol->set_buffer(buffer.raw(), RECEIVER_BUFFER_SIZE);

    Channel::setup();
  }

  int error_count() const { return _protocol->errors(); }
  int skip_count() const { return _protocol->skipped_message_count; }

  void skip();

  template <class M>
  void send(const M &m) {
    _send(m);
  }
  void raw_send(const byte *bytes, const byte length) { _send(bytes, length); }
};
}  // namespace rs485