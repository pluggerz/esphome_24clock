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

  // variables below are set when we get an STX
  bool haveETX_;
  byte inputPos_;
  byte currentByte_;
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
    Serial.write((c << 4) | (c ^ 0x0F));

    // second nibble
    c = what & 0x0F;
    Serial.write((c << 4) | (c ^ 0x0F));
  }  // end of RS485::sendComplemented

 public:
  // reset to no incoming data (eg. after a timeout)
  void reset(const char *reason) override {
    LOGI(TAG, "reset: %s", reason);
    haveSTX_ = false;
    available_ = false;
    inputPos_ = 0;
    startTime_ = 0;
#ifdef DOLED
    Leds::set_ex(LED_CHANNEL_DATA, LedColors::red);
    Leds::set_ex(LED_CHANNEL_STATE, LedColors::purple);
    Leds::publish();
#endif
  }

  // reset
  void begin() {
    reset("begin");
    errorCount_ = 0;
  }

  // free memory in buf_
  void stop() { reset("stop"); }

  // handle incoming data, return true if packet ready
  bool update();

  inline void wait_for_room() {
    return;

#ifdef DOLOG
    bool waited = false;
#endif
    int av = Serial.availableForWrite();
    while (av < 20) {
#ifdef DOLOG
      if (!waited) {
        ESP_LOGW(TAG, "Waited for available: %d", av);
      }
      waited = true;
#endif
      delay(3);  // FIX CRC ERRORS ?
      av = Serial.availableForWrite();
    }
  }

  // send a message of "length" bytes (max 255) to other end
  // put STX at start, ETX at end, and add CRC
  inline void sendMsg(const byte *data, const byte length) override {
    wait_for_room();
    Serial.write(STX);  // STX
    for (byte i = 0; i < length; i++) {
      wait_for_room();
      sendComplemented(data[i]);
    }
    Serial.write(ETX);  // ETX
    sendComplemented(crc8(data, length));
  }  // end of RS485::sendMsg

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
  int available = Serial.available();
  if (available == 0) {
#ifdef DOLED
    Leds::set_ex(LED_CHANNEL_DATA, LedColors::red);
    // Leds::publish();
#endif
    return false;
  }
  ESP_LOGD(TAG, "RS485: available() = %d", available);

#ifdef DOLED
  Leds::set_ex(LED_CHANNEL_DATA, LedColors::green);
#endif

  // while (Serial.available() > 0)
  {
    byte inByte = Serial.read();
    switch (inByte) {
      case STX:  // start of text
        ESP_LOGD(TAG, "RS485: STX  (E=%ld)", errorCount_);
        haveSTX_ = true;
        haveETX_ = false;
        inputPos_ = 0;
        firstNibble_ = true;
        startTime_ = millis();
        break;

      case ETX:  // end of text (now expect the CRC check)
        ESP_LOGD(TAG, "RS485: ETX  (E=%ld)", errorCount_);
        haveETX_ = true;
        break;

      default:
        // wait until packet officially starts
        if (!haveSTX_) {
          ESP_LOGD(TAG, "ignoring %d (E=%ld)", (int)inByte, errorCount_);
          break;
        }
        ESP_LOGVV(TAG, "received nibble %d (E=%ld)", (int)inByte, errorCount_);
        // check byte is in valid form (4 bits followed by 4 bits complemented)
        if ((inByte >> 4) != ((inByte & 0x0F) ^ 0x0F)) {
          reset("invlaid nibble");
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

        // if we have the ETX this must be the CRC
        if (haveETX_) {
          if (crc8(buffer, inputPos_) != currentByte_) {
            reset("crc!");
            errorCount_++;
            ESP_LOGE(TAG, "CRC!? (E=%ld)", errorCount_);
            break;  // bad crc
          }         // end of bad CRC

          available_ = true;
#ifdef DOLED
          Leds::set_ex(LED_CHANNEL_DATA, LedColors::green);
#endif
          return true;  // show data ready
        }               // end if have ETX already

        // keep adding if not full
        if (inputPos_ < buffer_size) {
          ESP_LOGVV(TAG, "received byte %d (E=%ld)", (int)currentByte_,
                    errorCount_);
          buffer[inputPos_++] = currentByte_;
        } else {
          reset("overflow");  // overflow, start again
          errorCount_++;
          ESP_LOGE(TAG, "OVERFLOW !? (E=%ld)", errorCount_);
        }

        break;

    }  // end of switch
  }    // end of while incoming data

  return false;  // not ready yet
}  // end of Protocol::update

using rs485::Protocol;

Protocol *Protocol::create_default() { return &basic_protocol; }

void Channel::_send(const byte *bytes, const byte length) {
  LOGV(TAG, "Channel::_send(%d bytes)", length);
  gate.start_transmitting();
  _protocol->sendMsg(bytes, length);
}

Millis last_channel_process_ = 0;

bool Channel::ready() const { return Serial.availableForWrite() > 32; }

void Channel::loop() {
  if (_baudrate == 0) {
#ifdef DOLED
    Leds::set_ex(LED_CHANNEL_STATE, LedColors::orange);
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
#define alternative 10
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