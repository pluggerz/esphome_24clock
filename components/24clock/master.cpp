#include "stub.h"
#if defined(MASTER)

#include "master.h"
#include "pins.h"

#include "esphome/core/log.h"
using namespace esphome;

static const char *const TAG = "master";

OneWire<SYNC_IN_PIN, SYNC_OUT_PIN> wire;

clock24::Master::Master()
{
}

void clock24::Master::dump_config()
{
    ESP_LOGCONFIG(TAG, "Master:");
}

class WireSender
{
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
        if (now - last < 2000)
        {
            return;
        }
        last = now;
        blinking = !blinking;
        ESP_LOGI(TAG, "Wire out: %s", blinking ? "HIGH" : "LOW");
        wire.set(blinking);
    }
} wire_controller;

void clock24::Master::setup()
{
    wire.setup();

    ESP_LOGI(TAG, "Master: setup!");

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
}

void clock24::Master::loop()
{
    wire_controller.blink();
    if (wire.pending())
    {
        bool state = wire.flush();
        ESP_LOGI(TAG, "Wire in: %s", state ? "HIGH" : "LOW");
    }
}

#endif
