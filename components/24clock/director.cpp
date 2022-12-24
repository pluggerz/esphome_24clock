#include "stub.h"
#if defined(MASTER)

#include "pinio.h"

#include "esphome/core/helpers.h"

esphome::HighFrequencyLoopRequester highFrequencyLoopRequester;

#include "esphome/core/log.h"

using namespace esphome;

#include "director.h"
#include "pins.h"
#include "channel.h"
#include "channel.interop.h"

static const char *const TAG = "controller";

using clock24::Director;

namespace Hal
{
    void yield()
    {
    }
} // namespace Hal

Director::Director()
{
}

onewire::RxOnewire rx;

onewire::TxOnewire underlying_tx(onewire::TX_BAUD);
onewire::BufferedTxOnewire<5> tx(&underlying_tx);

#define RECEIVER_BUFFER_SIZE 128

class DirectorChannel : public rs485::BufferChannel<RECEIVER_BUFFER_SIZE>
{
public:
    DirectorChannel()
    {
        baudrate(9600);
    }

    virtual void process(const byte *bytes, const byte length) override
    {
    }
} channel;

const char *cmd2string(const CmdEnum &cmd)
{
    switch (cmd)
    {
    case PERFORMER_ONLINE:
        return "ONLINE";
    case DIRECTOR_ACCEPT:
        return "ACCEPT";
    case PERFORMER_PREPPING:
        return "PREPPING";
    case DIRECTOR_PING:
        return "PING";
    case TOCK:
        return "TOCK";
    default:
        return "UNKNOWN";
    }
}

void logd_cmd(const char *what, const OneCommand &cmd)
{
    ESP_LOGD(TAG, "%s: {%d: %d - %s} %d", what, cmd.msg.src, cmd.msg.cmd, cmd2string(static_cast<CmdEnum>(cmd.msg.cmd)), cmd.raw);
}

void logw_cmd(const char *what, const OneCommand &cmd)
{
    ESP_LOGW(TAG, "%s: {%d: %d - %s} %d", what, cmd.msg.src, cmd.msg.cmd, cmd2string(static_cast<CmdEnum>(cmd.msg.cmd)), cmd.raw);
}

void logi_cmd(const char *what, const OneCommand &cmd)
{
    ESP_LOGI(TAG, "%s: {%d: %d - %s} %d", what, cmd.msg.src, cmd.msg.cmd, cmd2string(static_cast<CmdEnum>(cmd.msg.cmd)), cmd.raw);
}

void transmit(OneCommand command)
{
    logi_cmd("Send", command);
    tx.transmit(command.raw);
}

void Director::dump_config()
{
    dumped = true;
    ESP_LOGCONFIG(TAG, "Master:");
    ESP_LOGCONFIG(TAG, "  pin_mode: %s", PIN_MODE);
    ESP_LOGCONFIG(TAG, "  performers: %d", _performers);
    ESP_LOGCONFIG(TAG, "  is_high_frequency: %s", esphome::HighFrequencyLoopRequester::is_high_frequency() ? "YES" : "NO!?");
    ESP_LOGCONFIG(TAG, "  message sizes (in bytes):");
    ESP_LOGCONFIG(TAG, "     Msg:    %d", sizeof(OneCommand::Msg));
    ESP_LOGCONFIG(TAG, "     Accept: %d", sizeof(OneCommand::Accept));
    tx.dump_config();
    channel.dump_config();
}

class WireSender
{
    int value = 0;
    bool blinking = false;
    uint32_t last = 0;
    enum Mode
    {
        Blink
    };
    Mode mode = Blink;

public:
    void blink()
    {
        auto now = millis();
        if (now - last < 2500)
        {
            return;
        }
        last = now;
        tx.transmit(value++);
        // blinking = !blinking;
        // ESP_LOGI(TAG, "Wire out: %s", blinking ? "HIGH" : "LOW");
        // wire.set(blinking);
        // int value = 'A';
        // ESP_LOGI(TAG, "Wire out: %d", value);
        // OneWireProtocol::write(value);
    }
} wire_controller;

class AcceptAction : public DelayAction
{
    bool active = false;

public:
    void start()
    {
        active = true;
    }

    void stop()
    {
        active = false;
    }

    virtual void update() override
    {
        if (!active)
            return;

        auto msg = OneCommand::Accept::create(channel.baudrate());
        transmit(msg);
        ESP_LOGI(TAG, "transmit: Accept(%dbaud)", msg.accept.baudrate);
    }
} accept_action;

class PingOneWireAction : public DelayAction
{
    bool send = false;
    Millis t0;

public:
    PingOneWireAction() : DelayAction(5000)
    {
    }
    virtual void update() override
    {
        if (!send)
        {
            send = true;
            t0 = millis();
            transmit(OneCommand::ping());
        }
        else
        {
            send = false;
            ESP_LOGW(TAG, "(via onewire:) Ping lost !?");
        }
    }

    void received(int _performers)
    {
        auto delay = millis() - t0;
        if (_performers > 0 && _performers != 24)
            ESP_LOGI(TAG, "(via onewire:) Performer PING duration: %dmillis (projected delay: %d)", delay, delay * 24 / _performers);
        else
            ESP_LOGI(TAG, "(via onewire:) Performer PING duration: %dmillis", delay);
        send = false;
    }
} ping_onewire_action;

class TickChannelAction : public DelayAction
{
    bool send = false;
    Millis t0;

public:
    TickChannelAction() : DelayAction(500)
    {
    }
    virtual void update() override
    {
        if (!send)
        {
            send = true;
            t0 = millis();
            channel.send(ChannelMessage::tick());
        }
        else
        {
            send = false;
            ESP_LOGW(TAG, "(via channel:) Tick lost !?");
        }
    }

    void received(int _performers)
    {
        auto delay = millis() - t0;
        if (_performers > 0 && _performers != 24)
            ESP_LOGI(TAG, "(via channel:) Performer TICK TOCK duration: %dmillis (projected delay: %d)", delay, delay * 24 / _performers);
        else
            ESP_LOGI(TAG, "(via channel:) Performer TICK TOCK duration: %dmillis", delay);
        send = false;
    }
} tick_action;

class TestOnewireAction : public DelayAction
{
    bool send = false;
    Millis t0;

public:
    TestOnewireAction() : DelayAction(1000)
    {
    }
    virtual void update() override
    {
        if (tx.transmitted())
        {
            tx.transmit(3);
        }
    }
} test_onewire_action;

void Director::setup()
{
    ESP_LOGI(TAG, "Master: setup!");
    highFrequencyLoopRequester.start();

    pinMode(LED_BUILTIN, OUTPUT);

    // what to do with the following pins ?
    // ?? const int MASTER_GPI0_PIN  = 0; // GPI00
    pinMode(MASTER_GPI0_PIN, INPUT);
    // pinMode(ESP_TXD_PIN, OUTPUT);
    // pinMode(ESP_RXD_PIN, INPUT);
    pinMode(I2C_SDA_PIN, INPUT);
    pinMode(I2C_SCL_PIN, INPUT);

    pinMode(GPIO_14, INPUT);
    pinMode(USB_POWER_PIN, INPUT);

    channel.setup();
    channel.start_transmitting();

    rx.setup();
    rx.begin(onewire::RX_BAUD);

    tx.setup();

#if MODE >= MODE_ONEWIRE_INTERACT
    accept_action.start();
#endif
}

#if MODE == MODE_ONEWIRE_PASSTROUGH || MODE == MODE_ONEWIRE_MIRROR
onewire::Value value = 0;

void Director::loop()
{
    if (!dumped)
        return;

    Micros now = micros();
    rx.loop(now);
    tx.loop(now);
    if (rx.pending())
    {
        auto value = rx.flush();
        ESP_LOGI(TAG, "RECEIVED: {%d}", value);
    }
    if (tx.transmitted())
    {
        ESP_LOGI(TAG, "TRANSMIT: {value=%d}", value);
        tx.transmit(value++);
        if (value > 250)
            value = 0;
    }
}
#endif

/*
#if MODE == MODE_CHANNEL
void Director::loop()
{
    if (!dumped)
        return;
    channel.loop();
    tick_action.loop();
}
#endif
*/

#if MODE == MODE_ONEWIRE_CMD || MODE == MODE_CHANNEL
int test_count_value = 0;
void Director::loop()
{
    if (!dumped)
        return;

#if MODE == MODE_CHANNEL
        // channel.loop();
        // tick_action.loop();
        // test_onewire_action.loop();
#endif

    Micros now = micros();
    // rx.loop(now);
    tx.loop(now);
    if (rx.pending())
    {
        OneCommand cmd;
        cmd.raw = rx.flush();
        if (cmd.raw == 100)
        {
            ESP_LOGI(TAG, "  GOT: %d", cmd.raw);
        }
        // logw_cmd("RECEIVED", cmd);
        // if (cmd.msg.cmd == CmdEnum::DIRECTOR_ACCEPT)
        //     ESP_LOGI(TAG, "  -> Accept(%d)", cmd.accept.baudrate);
    }
    if (tx.transmitted())
    {
        tx.transmit(test_count_value++);
        if (test_count_value > 300)
            test_count_value = 0;

        // OneCommand cmd = OneCommand::Accept::create(0xFF);
        // logw_cmd("TRANSMIT ACCEPT(140)", cmd);
        // transmit(cmd);
    }
}
#endif

#if MODE >= MODE_ONEWIRE_INTERACT

bool pheripal_online = true;

void Director::loop()
{
    if (_killed)
        return;

    if (!dumped)
        return;

    accept_action.loop();
    ping_onewire_action.loop();
    tick_action.loop();

    Micros now = micros();
    rx.loop(now);
    tx.loop(now);
    channel.loop();

    if (rx.pending())
    {
        OneCommand cmd;
        cmd.raw = rx.flush();
        logi_cmd("Received", cmd);
        switch (cmd.msg.cmd)
        {
        case CmdEnum::DIRECTOR_PING:
        {
            ping_onewire_action.received(_performers);
            break;
        }

        case CmdEnum::PERFORMER_ONLINE:
            accept_action.start();
            break;

        case CmdEnum::TOCK:
            tick_action.received(cmd.msg.src);
            break;

        case CmdEnum::DIRECTOR_ACCEPT:
        {
            if (cmd.from_master())
            {
                _performers = 0;
                ESP_LOGI(TAG, "DIRECTOR_ACCEPT(%d): No performers ? Make sure, one is not MODE_ONEWIRE_PASSTROUGH!", cmd.accept.baudrate);
            }
            else
            {
                accept_action.stop();

                _performers = cmd.msg.src + 1;
                ESP_LOGI(TAG, "DIRECTOR_ACCEPT|(%d): Total performers: %d", cmd.accept.baudrate, _performers);
            }
        }
        break;

        default:
            // ignore
            logw_cmd("IGNORED", cmd);
        }
    }
}

#endif

void Director::kill()
{
    ESP_LOGW(TAG, "OTA detected, will kill me...");

    tx.kill();
    rx.kill();
    _killed = true;
}

#endif