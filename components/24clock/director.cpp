#include "stub.h"
#if defined(MASTER)

#include "esphome/core/helpers.h"

esphome::HighFrequencyLoopRequester highFrequencyLoopRequester;

#include "esphome/core/log.h"

using namespace esphome;

#include "director.h"
#include "pins.h"

static const char *const TAG = "controller";

clock24::Master::Master()
{
}

onewire::RxOnewire rx;

onewire::TxOnewire underlying_tx(onewire::TX_BAUD);
onewire::BufferedTxOnewire<5> tx(&underlying_tx);

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
    default:
        return "UNKNOWN";
    }
}

void logd_cmd(const char *what, const OneCommand &cmd)
{
    ESP_LOGD(TAG, "%s: {%d: %d - %s}", what, cmd.msg.src, cmd.msg.cmd, cmd2string(static_cast<CmdEnum>(cmd.msg.cmd)));
}

void logw_cmd(const char *what, const OneCommand &cmd)
{
    ESP_LOGW(TAG, "%s: {%d: %d - %s}", what, cmd.msg.src, cmd.msg.cmd, cmd2string(static_cast<CmdEnum>(cmd.msg.cmd)));
}

void transmit(OneCommand command)
{
    logd_cmd("Send", command);
    tx.transmit(command.raw);
}

void clock24::Master::dump_config()
{
    dumped = true;
    ESP_LOGCONFIG(TAG, "Master:");
    ESP_LOGCONFIG(TAG, "  performers: %d", _performers);
    ESP_LOGCONFIG(TAG, "  is_high_frequency: %s", esphome::HighFrequencyLoopRequester::is_high_frequency() ? "YES" : "NO!?");
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

        transmit(OneCommand::accept());
    }
} accept_action;

class PingAction : public DelayAction
{
    bool send = false;
    Millis t0;

public:
    PingAction() : DelayAction(5000)
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
            ESP_LOGW(TAG, "Ping lost !?");
        }
    }

    void received(int _performers)
    {
        auto delay = millis() - t0;
        if (_performers > 0 && _performers != 24)
            ESP_LOGI(TAG, "Performer PING duration: %dmillis (projected delay: %d)", delay, delay * 24 / _performers);
        else
            ESP_LOGI(TAG, "Performer PING duration: %dmillis", delay);
        send = false;
    }
} ping_action;

void clock24::Master::setup()
{
    ESP_LOGI(TAG, "Master: setup!");
    highFrequencyLoopRequester.start();

    pinMode(LED_BUILTIN, OUTPUT);

    // what to do with the following pins ?
    // ?? const int MASTER_GPI0_PIN  = 0; // GPI00
    pinMode(MASTER_GPI0_PIN, INPUT);
    pinMode(ESP_TXD_PIN, OUTPUT);
    pinMode(ESP_RXD_PIN, INPUT);
    pinMode(I2C_SDA_PIN, INPUT);
    pinMode(I2C_SCL_PIN, INPUT);

    pinMode(GPIO_14, INPUT);
    pinMode(USB_POWER_PIN, INPUT);

    rx.setup();
    rx.begin(onewire::RX_BAUD);
    // rx.async = false;

    tx.setup();
    // tx.disable_async();

#if MODE >= MODE_ONEWIRE_INTERACT
    accept_action.start();
#endif
}

#if MODE == MODE_ONEWIRE_PASSTROUGH || MODE == MODE_ONEWIRE_MIRROR
onewire::Value value = 0;

void clock24::Master::loop()
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

#if MODE == MODE_ONEWIRE_CMD
void clock24::Master::loop()
{
    if (!dumped)
        return;

    Micros now = micros();
    rx.loop(now);
    tx.loop(now);
    if (rx.pending())
    {
        OneCommand cmd;
        cmd.raw = rx.flush();
        logw_cmd("RECEIVED", cmd);
    }
    if (tx.transmitted())
    {
        OneCommand cmd = OneCommand::accept();
        logw_cmd("TRANSMIT", cmd);
        transmit(cmd);
    }
}
#endif

#if MODE >= MODE_ONEWIRE_INTERACT

bool pheripal_online = true;

void clock24::Master::loop()
{
    if (!dumped)
        return;

    accept_action.loop();
    ping_action.loop();

    Micros now = micros();
    rx.loop(now);
    tx.loop(now);
    if (rx.pending())
    {
        OneCommand cmd;
        cmd.raw = rx.flush();
        logd_cmd("Received", cmd);
        switch (cmd.msg.cmd)
        {
        case CmdEnum::DIRECTOR_PING:
        {
            ping_action.received(_performers);
            break;
        }

        case CmdEnum::PERFORMER_ONLINE:
            ESP_LOGI(TAG, "Performer ONLINE");
            accept_action.start();
            break;

        case CmdEnum::DIRECTOR_ACCEPT:
        {
            ESP_LOGI(TAG, "Performer ONLINE");
            accept_action.stop();
            _performers = cmd.msg.src + 1;
            ESP_LOGI(TAG, "Total performers: %d", _performers);
        }
        break;

        default:
            // ignore
            logw_cmd("IGNORED", cmd);
        }
    }
}

#endif

#endif