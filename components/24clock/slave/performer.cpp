#include "stub.h"

#include "onewire.h"
#include "pins.h"
#include "leds.h"

// OneWireProtocol protocol;
onewire::RxOnewire rx;

onewire::TxOnewire underlying_tx(onewire::TX_BAUD);
onewire::BufferedTxOnewire<5> tx(&underlying_tx);

OneWireTracker tracker;

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

class InitializeAction : public DelayAction
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
                transmit(cmd.next_source());
                performer_id = cmd.msg.src;
                set_action(&default_action);
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
    Leds::setup();

    // tracker.setup();

    Leds::set_all(rgb_color(0xFF, 0xFF, 0xFF));
    Leds::publish();

    rx.setup();
    rx.begin(onewire::RX_BAUD);

    tx.setup();

#ifdef APA102_USE_FAST_GPIO
    Leds::set(LED_COUNT - 1, rgb_color(0xFF, 0xFF, 0x00));
#else
    Leds::set(LED_COUNT - 1, rgb_color(0x00, 0xFF, 0xFF));
#endif

#if MODE >= MODE_ONEWIRE_INTERACT
    set_action(&reset_action);
#endif
}

LoopFunction current = reset_mode;

#if MODE == MODE_ONEWIRE_PASSTROUGH
void loop()
{
    // for debugging
    Millis now = millis();
    if (now - t0 > 10)
    {
        t0 = now;
        Leds::publish();
    }

    if (tracker.pending())
    {
        tracker.replicate();
    }
}
#endif

#if MODE >= MODE_ONEWIRE_MIRROR
void loop()
{
    // for debugging
    Millis now = millis();
    if (now - t0 > 25)
    {
        t0 = now;
        Leds::publish();
    }

    Micros now_in_micros = micros();
    tx.loop(now_in_micros);
    rx.loop(now_in_micros);
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