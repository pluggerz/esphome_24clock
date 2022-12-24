#include "stub.h"

#include "channel.h"

#ifdef ESP8266

#include "esphome/core/log.h"
using namespace esphome;

const char *const TAG = "channel";

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
using rs485::Gate;


// Helper class to calucalte CRC8
class CRC8
{
public:
    static byte calc(const byte *addr, byte len)
    {
        byte crc = 0;
        while (len--)
        {
            byte inbyte = *addr++;
            for (byte i = 8; i; i--)
            {
                byte mix = (crc ^ inbyte) & 0x01;
                crc >>= 1;
                if (mix)
                    crc ^= 0x8C;
                inbyte >>= 1;
            } // end of for
        }     // end of while
        return crc;
    }
};

// based on: http://www.gammon.com.au/forum/?id=11428

class BasicProtocol : public rs485::Protocol
{
public:
    enum
    {
        STX = '\2', // start of text
        ETX = '\3'  // end of text
    };              // end of enum

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
    inline void sendComplemented(const byte what)
    {
        byte c;

        // first nibble
        c = what >> 4;
        Serial.write((c << 4) | (c ^ 0x0F));
        Hal::yield();

        // second nibble
        c = what & 0x0F;
        Serial.write((c << 4) | (c ^ 0x0F));
        Hal::yield();
    } // end of RS485::sendComplemented

public:
    // reset to no incoming data (eg. after a timeout)
    void reset(const char *reason) override
    {
#ifdef DOLOG
        ESP_LOGI(TAG, "reset: %s", reason);
#endif
        haveSTX_ = false;
        available_ = false;
        inputPos_ = 0;
        startTime_ = 0;
    }

    // reset
    void begin()
    {
        reset("begin");
        errorCount_ = 0;
    }

    // free memory in buf_
    void stop()
    {
        reset("stop");
    }

    // handle incoming data, return true if packet ready
    bool update();

    inline void wait_for_room()
    {
        return;

#ifdef DOLOG
        bool waited = false;
#endif
        int av = Serial.availableForWrite();
        while (av < 20)
        {
#ifdef DOLOG
            if (!waited)
            {
                ESP_LOGW(TAG, "Waited for available: %d", av);
            }
            waited = true;
#endif
            delay(3); // FIX CRC ERRORS ?
            av = Serial.availableForWrite();
        }
    }

    // send a message of "length" bytes (max 255) to other end
    // put STX at start, ETX at end, and add CRC
    inline void sendMsg(const byte *data, const byte length) override
    {
        wait_for_room();
        Serial.write(STX); // STX
        Hal::yield();
        for (byte i = 0; i < length; i++)
        {
            wait_for_room();
            sendComplemented(data[i]);
        }
        Serial.write(ETX); // ETX
        Hal::yield();
        sendComplemented(crc8(data, length));
    } // end of RS485::sendMsg

    // returns true if packet available
    bool available() const
    {
        return available_;
    };

    /*
    byte *getData() const
    {
        return buffer.raw();
    }*/

    byte getLength() const
    {
        return inputPos_;
    }

    // return how many errors we have had
    unsigned long getErrorCount() const
    {
        return errorCount_;
    }

    // return when last packet started
    unsigned long getPacketStartTime() const
    {
        return startTime_;
    }

    // return true if a packet has started to be received
    bool isPacketStarted() const
    {
        return haveSTX_;
    }

    void set_buffer(byte *_buffer, const int length) override
    {
        buffer = _buffer;
        buffer_size = length;
    }

} basic_protocol; // end of class RS485Protocol

bool BasicProtocol::update()
{
    int available = Serial.available();
    if (available == 0)
    {
#ifdef DOLED
        Leds::set(DATALED, rgb_color(0xFF, 0x00, 0x00));
        // Leds::publish();
#endif
        return false;
    }
#ifdef DOLOG
    ESP_LOGD(TAG, "RS485: available() = %d", available);
#endif

#ifdef DOLED
    Leds::set(LED_CHANNEL, rgb_color(0xFF, 0xFF, 0x00));

    Leds::set(DATALED, rgb_color(0xFF, 0x00, 0xFF));
    // Leds::publish();
#endif

    // while (Serial.available() > 0)
    {
        byte inByte = Serial.read();
        switch (inByte)
        {
        case STX: // start of text
#ifdef DOLOG
            ESP_LOGD(TAG, "RS485: STX  (E=%ld)", errorCount_);
#endif
            haveSTX_ = true;
            haveETX_ = false;
            inputPos_ = 0;
            firstNibble_ = true;
            startTime_ = millis();
            break;

        case ETX: // end of text (now expect the CRC check)
#ifdef DOLOG
            ESP_LOGD(TAG, "RS485: ETX  (E=%ld)", errorCount_);
#endif
            haveETX_ = true;
            break;

        default:
            // wait until packet officially starts
            if (!haveSTX_)
            {
#ifdef DOLOG
                ESP_LOGD(TAG, "ignoring %d (E=%ld)", (int)inByte, errorCount_);
#endif
                break;
            }
#ifdef DOLOG
            ESP_LOGVV(TAG, "received nibble %d (E=%ld)", (int)inByte, errorCount_);
#endif
            // check byte is in valid form (4 bits followed by 4 bits complemented)
            if ((inByte >> 4) != ((inByte & 0x0F) ^ 0x0F))
            {
                reset("invlaid nibble");
                errorCount_++;
#ifdef DOLOG
                ESP_LOGE(TAG, "invalid nibble !? (E=%ld)", errorCount_);
#endif
                break; // bad character
            }          // end if bad byte

            // convert back
            inByte >>= 4;

            // high-order nibble?
            if (firstNibble_)
            {
                currentByte_ = inByte;
                firstNibble_ = false;
                break;
            } // end of first nibble

            // low-order nibble
            currentByte_ <<= 4;
            currentByte_ |= inByte;
            firstNibble_ = true;

            // if we have the ETX this must be the CRC
            if (haveETX_)
            {
                if (crc8(buffer, inputPos_) != currentByte_)
                {
                    reset("crc!");
                    errorCount_++;
#ifdef DOLOG
                    ESP_LOGE(TAG, "CRC!? (E=%ld)", errorCount_);
#endif
                    break; // bad crc
                }          // end of bad CRC

                available_ = true;
                return true; // show data ready
            }                // end if have ETX already

            // keep adding if not full
            if (inputPos_ < buffer_size)
            {
#ifdef DOLOG
                ESP_LOGVV(TAG, "received byte %d (E=%ld)", (int)currentByte_, errorCount_);
#endif
                buffer[inputPos_++] = currentByte_;
            }
            else
            {
                reset("overflow"); // overflow, start again
                errorCount_++;
#ifdef DOLOG
                ESP_LOGE(TAG, "OVERFLOW !? (E=%ld)", errorCount_);
#endif
            }

            break;

        } // end of switch
    }     // end of while incoming data

    return false; // not ready yet
} // end of Protocol::update

using rs485::Protocol;

Protocol *Protocol::create_default()
{
    return &basic_protocol;
}

void Gate::dump_config()
{
#ifdef DOLOG
    ESP_LOGCONFIG(TAG, "  rs485::Gate");
    ESP_LOGCONFIG(TAG, "     de_pin: %d", RS485_DE_PIN);
    ESP_LOGCONFIG(TAG, "     re_pin: %d", RS485_RE_PIN);
    ESP_LOGCONFIG(TAG, "     state: %d", state);
#endif
}

void Gate::setup()
{
    pinMode(RS485_DE_PIN, OUTPUT);
    pinMode(RS485_RE_PIN, OUTPUT);
#ifdef DOLOG
    ESP_LOGI(TAG, "state: setup");
#endif
}

void Gate::start_receiving()
{
    if (state == RECEIVING)
        return;

    // make sure we are done with sending
    Serial.flush();

    digitalWrite(RS485_DE_PIN, LOW);
    digitalWrite(RS485_RE_PIN, LOW);

    state = RECEIVING;
#ifdef DOLOG
    ESP_LOGI(TAG, "state: RECEIVING");
#endif
}
void Gate::start_transmitting()
{
    if (state == TRANSMITTING)
        return;

    digitalWrite(RS485_DE_PIN, HIGH);
    digitalWrite(RS485_RE_PIN, LOW);

    state = TRANSMITTING;
#ifdef DOLOG
    ESP_LOGI(TAG, "state: TRANSMITTING");
#endif
}

void Channel::baudrate(Baudrate baud_rate)
{
    if (_baudrate)
        Serial.end();
    _baudrate = baud_rate;
    Serial.begin(_baudrate);

#ifdef DOLOG
    ESP_LOGI(TAG, "state: Serial.begin(%d)", baud_rate);
#endif
}

void Channel::_send(const byte *bytes, const byte length)
{
#ifdef DOLOG
    ESP_LOGI(TAG, "_send(%d bytes)", length);
#endif
    gate.start_transmitting();
    _protocol->sendMsg(bytes, length);
}

Millis last_channel_process_ = 0;

void Channel::loop()
{
    if (_baudrate == 0)
    {
#ifdef DOLED
        Leds::set(LED, rgb_color(0x00, 0x00, 0xFF));
#endif
        return;
    }

    if (!gate.is_receiving() || !_protocol)
    {
#ifdef DOLED
        Leds::set(LED, rgb_color(0xFF, 0x00, 0x00));
#endif
        return;
    }

#ifdef DOLED
#define alternative 10
    if (last_channel_process_ && millis() - last_channel_process_ > 50)
    {

        Leds::set(alternative, rgb_color(0x00, 0x00, 0xFF));
        Leds::publish();
        last_channel_process_ = 0;
    }
    else
    {
        Leds::set(alternative, rgb_color(0xFF, 0xFF, 0x00));
    }
#endif

    if (!((BasicProtocol *)_protocol)->update())
    {
#ifdef DOLED
        Leds::set(LED, rgb_color(0xFF, 0xFF, 0x00));
#endif
        return;
    }
#ifdef DOLED

    Leds::set(alternative, rgb_color(0x00, 0x4F, 0xFF));
    Leds::set(LED, rgb_color(0x00, 0xFF, 0x00));
    Leds::publish();

    last_channel_process_ = millis();
#endif // DOLED
    // TODO: cleanup
    BasicProtocol *pr = (BasicProtocol *)_protocol;
    process(pr->buffer, pr->getLength());
}