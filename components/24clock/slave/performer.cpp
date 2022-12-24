#include "stub.h"

#include "pins.h"
#include "pinio.h"
#include "onewire.h"
#include "channel.h"
#include "channel.interop.h"
#include "leds.h"

// OneWireProtocol protocol;
onewire::RxOnewire rx;

onewire::TxOnewire underlying_tx(onewire::BAUD);
onewire::BufferedTxOnewire<5> tx(&underlying_tx);
rs485::Gate gate;

#define RECEIVER_BUFFER_SIZE 128

class PerformerChannel : public rs485::BufferChannel<RECEIVER_BUFFER_SIZE>
{
public:
    void process(const byte *bytes, const byte length) override;
} channel;

Micros t0 = 0;
int performer_id = -1;

void reset_mode()
{
}

typedef void (*LoopFunction)();
typedef void (*ListenFunction)();

void transmit(const OneCommand &value)
{
    tx.transmit(value.raw);
}

void set_action(Action *action);

void show_action(const OneCommand &cmd)
{
    switch (cmd.msg.cmd)
    {
    case PERFORMER_ONLINE:
        Leds::set(LED_COUNT - 5, rgb_color(0x00, 0x00, 0xFF));
        break;

    case DIRECTOR_ACCEPT:
        Leds::set(LED_COUNT - 5, rgb_color(0x00, 0xFF, 0x00));
        break;

    case PERFORMER_PREPPING:
        Leds::set(LED_COUNT - 5, rgb_color(0x00, 0xFF, 0xFF));
        break;

    case DIRECTOR_PING:
        Leds::set(LED_COUNT - 5, rgb_color(0xFF, 0xFF, 0x00));
        break;

    default:
        Leds::set(LED_COUNT - 5, rgb_color(0xFF, 0x00, 0x00));
        break;
    }
    Leds::publish();
}

class DefaultAction : public DelayAction
{
public:
    virtual void update() override
    {
        // transmit(OneCommand::busy());
    }

    virtual void setup() override
    {
        DelayAction::setup();
        Leds::set(LED_COUNT - 2, rgb_color(0x00, 0xFF, 0x00));
    }

    virtual void loop() override
    {
        if (rx.pending())
        {
            OneCommand cmd;
            cmd.raw = rx.flush();
            show_action(cmd);
            transmit(cmd.next_source());
        }
        DelayAction::loop();
    }
} default_action;

#if MODE >= MODE_ONEWIRE_INTERACT
class ResetAction : public DelayAction
{
public:
    virtual void update() override
    {
        transmit(OneCommand::online());
    }

    virtual void setup() override
    {
        DelayAction::setup();
        Leds::set(LED_COUNT - 2, rgb_color(0xFF, 0x00, 0x00));
    }

    virtual void loop() override
    {
        // Leds::set(7, rgb_color(0xFF, 0xFF, 0x00));

        if (rx.pending())
        {
            OneCommand cmd;
            cmd.raw = rx.flush();
            show_action(cmd);
            switch (cmd.msg.cmd)
            {
            case PERFORMER_ONLINE:
                // we are already waiting, so ignore this one
                break;

            case DIRECTOR_ACCEPT:
                performer_id = cmd.msg.src;
                set_action(&default_action);

                transmit(cmd.next_source());

                // channel.baudrate(9600);
                channel.baudrate(cmd.accept.baudrate);
                channel.start_receiving();
                break;

            default:
                // just forward
                // tx.transmit(cmd.next_source().raw);
                break;
            }
        }
        DelayAction::loop();
    }
} reset_action;
#endif

Action *current_action = nullptr;

Micros t0_cmd;
void start_reset_cmd()
{
    t0_cmd = millis();
}

void set_action(Action *action)
{
    if (current_action != nullptr)
        current_action->final();
    current_action = action;
    if (current_action != nullptr)
        current_action->setup();
}

#if MODE == MODE_ONEWIRE_PASSTROUGH || MODE == MODE_CHANNEL

void follow_change()
{
    PIN_WRITE(SYNC_OUT_PIN, PIN_READ(SYNC_IN_PIN));
}
#endif

void setup()
{
    pinMode(SLAVE_RS485_TXD_DPIN, OUTPUT);
    pinMode(SLAVE_RS485_RXD_DPIN, INPUT);

    Leds::setup();

    // tracker.setup();

    Leds::set_all(rgb_color(0xFF, 0xFF, 0xFF));
    Leds::publish();

    rx.setup();
    rx.begin();

    tx.setup();

    channel.setup();

#if MODE == MODE_CHANNEL || MODE == MODE_ONEWIRE_CMD || MODE == MODE_ONEWIRE_PASSTROUGH
    channel.baudrate(9600);
    channel.start_receiving();
#endif

    gate.setup();
    gate.start_receiving();

#ifdef APA102_USE_FAST_GPIO
    Leds::set(LED_COUNT - 1, rgb_color(0xFF, 0xFF, 0x00));
#else
    Leds::set(LED_COUNT - 1, rgb_color(0x00, 0xFF, 0xFF));
#endif

#if MODE >= MODE_ONEWIRE_INTERACT
    set_action(&reset_action);
#endif

#if MODE == MODE_ONEWIRE_PASSTROUGH || MODE == MODE_CHANNEL
    auto interupt = digitalPinToInterrupt(SYNC_IN_PIN);
    if (interupt < 0)
    {
        Leds::set_all(rgb_color(0xFF, 0x00, 0x00));
        Leds::publish();
        while (true)
        {
        }
    }
    else
    {
        Leds::set_all(rgb_color(0x00, 0xFf, 0x00));
        Leds::publish();

        delay(2000);
    }
    attachInterrupt(interupt, follow_change, CHANGE);
#endif
}

LoopFunction current = reset_mode;

#if MODE == MODE_ONEWIRE_SLAVE_TRANSMITTER
namespace Hal
{
    void yield()
    {
    }
} // namespace Hal

void loop()
{
    Millis now = millis();
    if (now - t0 > 25)
    {
        t0 = now;
        Leds::publish();
    }

    tx.loop(micros()); // still needed, because of buffering :S
    if (tx.transmitted())
    {
        Leds::set(4, rgb_color(0xFf, 0xFF, 0x00));
        Leds::publish();

        delay(20);
        tx.transmit(128);

        Leds::set(4, rgb_color(0xFF, 0x00, 0xFF));
        Leds::publish();

        delay(20);
    }
}
#endif

#if MODE == MODE_ONEWIRE_PASSTROUGH || MODE == MODE_CHANNEL

namespace Hal
{
    void yield()
    {
    }
} // namespace Hal

void loop()
{
    // for debugging
    Millis now = millis();
    if (now - t0 > 25)
    {

        t0 = now;
        Leds::publish();
    }

    channel.loop();
    /*if (tracker.pending())
    {
        tracker.replicate();
    }*/
}
#endif

/*
#if MODE == MODE_CHANNEL
void loop()
{
    // for debugging
    Millis now = millis();
    if (now - t0 > 25)
    {
        Leds::set(0, PIN_READ(RS485_DE_PIN) ? rgb_color(0, 0xFF, 0) : rgb_color(0xff, 0, 0));
        Leds::set(1, PIN_READ(RS485_RE_PIN) ? rgb_color(0, 0xFF, 0) : rgb_color(0xff, 0, 0));

        t0 = now;
        Leds::publish();
    }

    Leds::publish();
    channel.loop();
}
#endif
*/

#if MODE >= MODE_ONEWIRE_MIRROR && MODE != MODE_CHANNEL && MODE != MODE_ONEWIRE_SLAVE_TRANSMITTER

namespace Hal
{
    void yield()
    {
    }
} // namespace Hal

void loop()
{
    // for debugging
    Millis now = millis();
    if (now - t0 > 25)
    {
        t0 = now;
        Leds::publish();
    }

    tx.loop();
    channel.loop();

    if (rx.pending())
    {
#if MODE == MODE_ONEWIRE_MIRROR
        auto value = rx.flush();
        tx.transmit(value);
#endif
#if MODE == MODE_ONEWIRE_CMD
        OneCommand cmd;
        cmd.raw = rx.flush();
        tx.transmit(cmd.next_source().raw);
#endif
    }
    if (current_action != nullptr)
        current_action->loop();
}
#endif

void PerformerChannel::process(const byte *bytes, const byte length)
{

    ChannelMessage *msg = (ChannelMessage *)bytes;
    switch (msg->getMsgEnum())
    {
    case ChannelMsgEnum::MSG_TICK:
        if (performer_id >= 0)
        {
            Leds::set(LED_CHANNEL, rgb_color(0xff, 0x00, 0xFF));
            Leds::publish();

            transmit(OneCommand::tock(performer_id));
        }
        break;

    default:
        Leds::set(LED_CHANNEL, rgb_color(0xff, 0x00, 0x00));
        Leds::publish();
        break;
    }
}