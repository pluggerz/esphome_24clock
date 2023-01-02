#pragma once

#include "../clocks_shared/pinio.h"
#include "pins.h"
#include "../clocks_shared/stub.h"

namespace Hal {
void yield();
}

namespace rs485 {

class Gate  // : public Gate
{
  enum State { RECEIVING = 1, TRANSMITTING = 2, NONE = 0 } state = NONE;

 public:
  void dump_config();

  bool is_receiving() const { return state == RECEIVING; }

  void setup();
  void start_receiving();
  void start_transmitting();
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

  void baudrate(Baudrate baud_rate);

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
};
}  // namespace rs485