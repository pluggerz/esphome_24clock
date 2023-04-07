// TODO: performer / director shared code, should go to SHARED

#include "../clocks_shared/channel.h"

#include "../clocks_shared/log.h"
#include "../clocks_shared/stub.h"

#ifdef ESP8266
#define DOLOG
#endif

#ifdef SLAVE
#define DOLED
#endif

#ifdef DOLED
#define LED 4
#define DATALED 5
#include "slave/leds.h"
#endif

using rs485::Channel;
using rs485::CRC8;
using rs485::Gate;
using rs485::TAG;

// based on: http://www.gammon.com.au/forum/?id=11428

class BasicProtocol : public rs485::Protocol {
 public:
  enum {
    STX = '\2',  // start of text
    ETX = '\3'   // end of text
  };             // end of enum

  // this is true once we have valid data in buf
  bool available_;

  // an STX (start of text) signals a packet start
  bool haveSTX_;

  // count of errors
  unsigned long errorCount_ = 0L;
  unsigned long errorCrcCount_ = 0L;

  // variables below are set when we get an STX
  int inputPos_;
  byte currentByte_;
  byte message_size;
  byte crc;
  bool firstNibble_;
  unsigned long startTime_;

  // data
  byte *buffer = nullptr;
  int buffer_size = 0;

  // helper private functions
  byte crc8(const byte *addr, byte len) { return CRC8::calc(addr, len); }

  // send a byte complemented, repeated
  // only values sent would be (in hex):
  //   0F, 1E, 2D, 3C, 4B, 5A, 69, 78, 87, 96, A5, B4, C3, D2, E1, F0
  inline void sendComplemented(const byte what) {
    byte c;

    // first nibble
    c = what >> 4;
    SerialDelegate.write((c << 4) | (c ^ 0x0F));

    // second nibble
    c = what & 0x0F;
    SerialDelegate.write((c << 4) | (c ^ 0x0F));
  }  // end of RS485::sendComplemented

 public:
  int errors() const override { return errorCount_; }

  // reset
  void begin() {
    haveSTX_ = false;
    errorCount_ = 0;
  }

  virtual void start_receiving() override { this->haveSTX_ = false; }

  // handle incoming data, return true if packet ready
  bool update();

  inline void wait_for_room() {
    // return;

    int av = SerialDelegate.availableForWrite();
    while (av < 42) {
      // delay(1);  // FIX CRC ERRORS ?
      av = SerialDelegate.availableForWrite();
    }
  }

  // send a message of "length" bytes (max 100) to other end
  // put STX at start, ETX at end, and add CRC
  inline void sendMsg(const byte *data, const byte length) override {
    if (length >= 100) {
      ESP_LOGW(TAG, "RS485: MAX 100 bytes !!  (E=%ld)", errorCount_);
      return;
    }
    // wait_for_room();
    SerialDelegate.write(STX);  // STX
    sendComplemented(length);
    sendComplemented(crc8(data, length));
    for (byte i = 0; i < length; i++) {
      // wait_for_room();
      sendComplemented(data[i]);
    }
    // wait_for_room();
    SerialDelegate.write(ETX);  // ETX
  }

  // returns true if packet available
  bool available() const { return available_; };

  /*
  byte *getData() const
  {
      return buffer.raw();
  }*/

  byte getLength() const { return inputPos_; }

  // return how many errors we have had
  unsigned long getErrorCount() const { return errorCount_; }

  // return when last packet started
  unsigned long getPacketStartTime() const { return startTime_; }

  // return true if a packet has started to be received
  bool isPacketStarted() const { return haveSTX_; }

  void set_buffer(byte *_buffer, const int length) override {
    buffer = _buffer;
    buffer_size = length;
  }

} basic_protocol;  // end of class RS485Protocol

bool BasicProtocol::update() {
  int available = SerialDelegate.available();
  if (available == 0) {
#ifdef DOLED
    Leds::set_ex(LED_CHANNEL_DATA, LedColors::red);
#endif
    return false;
  }
  ESP_LOGD(TAG, "RS485: available() = %d", available);

#ifdef DOLED
  Leds::set_ex(LED_CHANNEL_DATA, LedColors::green);
#endif

  while (SerialDelegate.available() > 0) {
    byte inByte = SerialDelegate.read();
    switch (inByte) {
      case STX:  // start of message
        ESP_LOGD(TAG, "RS485: STX  (E=%ld)", errorCount_);
        haveSTX_ = true;
        inputPos_ = -2;
        firstNibble_ = true;
        startTime_ = millis();
        message_size = 2;
        break;

      case ETX:  // end of text (now expect the CRC check)
        if (!haveSTX_) {
          // ignore, skip
          break;
        }
        if (inputPos_ != message_size) {
          ESP_LOGW(TAG, "%d(inputPos_) != %d(message_size)", inputPos_,
                   message_size);
          errorCount_++;
          haveSTX_ = false;
          break;
        }

        if (crc8(buffer, inputPos_) != crc) {
          errorCount_++;
          ESP_LOGE(TAG, "CRC!? (E=%ld)", errorCount_);
          haveSTX_ = false;
          break;  // bad crc
        }         // end of bad CRC
        available_ = true;
#ifdef DOLED
        Leds::set_ex(LED_CHANNEL_DATA, LedColors::green);
#endif
        return true;  // show data ready

      default:
        // wait until packet officially starts
        if (!haveSTX_) {
          ESP_LOGD(TAG, "ignoring %d (E=%ld)", (int)inByte, errorCount_);
          break;
        }

        if (inputPos_ == message_size) {
          ESP_LOGE(TAG, "EXPECTED ETX (E=%ld)", errorCount_);
          haveSTX_ = false;
          break;
        }

        ESP_LOGVV(TAG, "received nibble %d (E=%ld)", (int)inByte, errorCount_);
        // check byte is in valid form (4 bits followed by 4 bits complemented)
        if ((inByte >> 4) != ((inByte & 0x0F) ^ 0x0F)) {
          haveSTX_ = false;
          errorCount_++;
          ESP_LOGE(TAG, "invalid nibble !? (E=%ld)", errorCount_);
          break;  // bad character
        }         // end if bad byte

        // convert back
        inByte >>= 4;

        // high-order nibble?
        if (firstNibble_) {
          currentByte_ = inByte;
          firstNibble_ = false;
          break;
        }  // end of first nibble

        // low-order nibble
        currentByte_ <<= 4;
        currentByte_ |= inByte;
        firstNibble_ = true;

        // keep adding if not full
        if (inputPos_ >= buffer_size) {
          errorCount_++;
          haveSTX_ = false;
          ESP_LOGE(TAG, "OVERFLOW !? (E=%ld)", errorCount_);
          break;
        }

        ESP_LOGVV(TAG, "received byte %d (E=%ld)", (int)currentByte_,
                  errorCount_);
        if (inputPos_ == -2) {
          ESP_LOGVV(TAG, "REMARK SIZE: =%d", currentByte_);
          inputPos_ = -1;
          message_size = currentByte_;
          if (message_size >= 100) {
            errorCount_++;
            ESP_LOGE(TAG, "RS485: MAX 100 bytes !!  (E=%ld)", errorCount_);
            break;
          }
          break;
        } else if (inputPos_ == -1) {
          ESP_LOGVV(TAG, "CRC: =%d", currentByte_);
          crc = currentByte_;
          inputPos_ = 0;
          break;
        } else if (inputPos_ == 0 && is_skippable_message(currentByte_)) {
          skipped_message_count++;
          haveSTX_ = false;
          break;
        }
        buffer[inputPos_++] = currentByte_;
        break;

    }  // end of switch
  }    // end of while incoming data

  return false;  // not ready yet
}  // end of Protocol::update

using rs485::Protocol;

Protocol *Protocol::create_default() { return &basic_protocol; }

#ifdef MASTER
void Channel::_send(const byte *bytes, const byte length) {
  LOGD(TAG, "Channel::_send(%d bytes)", length);
  gate.start_transmitting();
  _protocol->sendMsg(bytes, length);
}
#endif

Millis last_channel_process_ = 0;

bool Channel::bytes_available_for_write(int bytes) const {
  return SerialDelegate.availableForWrite() > bytes;
}

void Channel::loop() {
  if (_baudrate == 0) {
#ifdef DOLED
    Leds::set_ex(LED_CHANNEL_STATE, LedColors::purple);
#endif
    return;
  }

  if (!gate.is_receiving() || !_protocol) {
#ifdef DOLED
    Leds::set_ex(LED_CHANNEL_STATE, LedColors::red);
#endif
    return;
  }

#ifdef DOLED
  if (last_channel_process_ && millis() - last_channel_process_ > 50) {
    Leds::set_ex(LED_CHANNEL_STATE, LedColors::blue);
    Leds::publish();

    last_channel_process_ = 0;
  } else {
    Leds::set_ex(LED_CHANNEL_STATE, LedColors::purple);
  }
#endif

  if (!((BasicProtocol *)_protocol)->update()) {
#ifdef DOLED
    Leds::set_ex(LED_CHANNEL_STATE, LedColors::orange);
#endif
    return;
  }
#ifdef DOLED
  Leds::set_ex(LED_CHANNEL_STATE, LedColors::green);
  Leds::publish();

  last_channel_process_ = millis();
#endif  // DOLED
  // TODO: cleanup
  BasicProtocol *pr = (BasicProtocol *)_protocol;
  process(pr->buffer, pr->getLength());
}