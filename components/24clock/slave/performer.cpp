#include "stub.h"

#include "pins.h"
#include "pinio.h"
#include "onewire.h"
#include "channel.h"
#include "channel.interop.h"
#include "leds.h"

// OneWireProtocol protocol;
onewire::RxOnewire rx;
onewire::TxOnewire tx;

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
#if MODE >= MODE_ONEWIRE_INTERACT

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

    virtual void loop() override;
} default_action;

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

    virtual void loop() override;
} reset_action;

void DefaultAction::loop()
{
    DelayAction::loop();

    Leds::set_ex(LED_MODE, LedColors::orange);

    if (!rx.pending())
        return;

    OneCommand cmd;
    cmd.raw = rx.flush();
    show_action(cmd);
    channel.loop();
    switch (cmd.msg.cmd)
    {
    case DIRECTOR_ONLINE:
        onewire::OnewireInterrupt::align();

        set_action(&reset_action);
        transmit(cmd.next_source());
        break;

    default:
        transmit(cmd.next_source());
        break;
    }
}

void ResetAction::loop()
{
    DelayAction::loop();

    Leds::set_ex(LED_MODE, LedColors::blue);

    // Leds::set(7, rgb_color(0xFF, 0xFF, 0x00));

    if (rx.pending())
    {
        OneCommand cmd;
        cmd.raw = rx.flush();
        show_action(cmd);
        switch (cmd.msg.cmd)
        {
        case DIRECTOR_ONLINE:
            onewire::OnewireInterrupt::align();
            transmit(cmd.next_source());
            break;

        case PERFORMER_ONLINE:
            // we are already waiting, so ignore this one
            break;

        case DIRECTOR_ACCEPT:
            performer_id = cmd.msg.src;
            set_action(&default_action);
            channel.baudrate(cmd.baudrate.speed);
            channel.start_receiving();

            transmit(cmd.next_source());
            break;

        default:
            // just forward
            transmit(cmd.next_source());
            break;
        }
    }
}

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

void setup()
{
    pinMode(SLAVE_RS485_TXD_DPIN, OUTPUT);
    pinMode(SLAVE_RS485_RXD_DPIN, INPUT);

    Leds::setup();
    Leds::blink(LedColors::green, 1);

    rx.setup();
#if MODE != MODE_ONEWIRE_PASSTROUGH
#ifdef DOLED
    Leds::set_ex(LED_ONEWIRE, LedColors::purple);
#endif
    rx.begin();
    Leds::blink(LedColors::green, 2);
#endif

    tx.setup();
#if MODE != MODE_ONEWIRE_PASSTROUGH
    tx.begin();
#endif
    Leds::blink(LedColors::green, 3);
    channel.setup();

#if MODE == MODE_CHANNEL || MODE == MODE_ONEWIRE_CMD || MODE == MODE_ONEWIRE_PASSTROUGH
    channel.baudrate(9600);
    channel.start_receiving();
    Leds::blink(LedColors::green, 4);
#endif

    gate.setup();
    gate.start_receiving();
    Leds::blink(LedColors::green, 5);

#ifndef APA102_USE_FAST_GPIO
    Leds::error(LEDS_ERROR_MISSING_APA102_USE_FAST_GPIO);
#endif

#if MODE >= MODE_ONEWIRE_INTERACT
    set_action(&reset_action);
#endif
}

LoopFunction current = reset_mode;

#if MODE == MODE_ONEWIRE_PASSTROUGH

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
}
#endif

#if MODE >= MODE_ONEWIRE_MIRROR

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
            transmit(OneCommand::tock(performer_id));
        }
        break;

    default:
        // Leds::set_ex(LED_CHANNEL, rgb_color(0xff, 0x00, 0x00));
        // Leds::publish();
        break;
    }
}