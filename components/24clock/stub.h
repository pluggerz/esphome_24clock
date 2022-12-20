#pragma once

#define MODE_ONEWIRE_PASSTROUGH 1
#define MODE_ONEWIRE_MIRROR 2
#define MODE_ONEWIRE_CMD 3
#define MODE_ONEWIRE_INTERACT 4

#define MODE MODE_ONEWIRE_PASSTROUGH

typedef unsigned long Micros;
typedef unsigned long Millis;

#ifdef ESP8266
#define MASTER

#include "stub.master.h"

#else
#define SLAVE

#include "slave/stub.slave.h"

#endif

constexpr int SRC_BITS = 6; // 24 handles, so 2^5 = 32 should fit
constexpr int SRC_MASTER = (1 << SRC_BITS) - 1;
constexpr int SRC_UNKNOWN = (1 << SRC_BITS) - 2;
constexpr int CMD_BITS = 3;
constexpr int RESERVED_BITS = 32 - SRC_BITS - CMD_BITS;

enum CmdEnum
{
    PERFORMER_ONLINE,
    DIRECTOR_ACCEPT,
    PERFORMER_PREPPING,
    DIRECTOR_PING,
};

// to be placed in proper .h file
union OneCommand
{
    uint32_t raw;
    struct Msg
    {
        // TODO: seperate in src/cmd
        uint32_t cmd : CMD_BITS;
        uint32_t src : SRC_BITS;
        uint32_t reserved : RESERVED_BITS;
    } __attribute__((packed, aligned(1))) msg;

    static OneCommand online();
    static OneCommand accept();
    static OneCommand ping();

    OneCommand next_source() const
    {
        OneCommand ret;
        ret.raw = raw;
        if (msg.src == SRC_MASTER)
            ret.msg.src = 0;
        else if (msg.src != SRC_UNKNOWN)
            ret.msg.src++;
        return ret;
    }
} __attribute__((packed, aligned(1)));

class Action
{
protected:
    Micros t0;

public:
    virtual void setup()
    {
        t0 = millis();
    };
    virtual void loop(){};
    virtual void final(){};
};

class DelayAction : public Action
{
    Micros _last = 0, _delay;

protected:
    virtual void first_update()
    {
        update();
    }

    virtual void update() = 0;
    DelayAction(Micros delay = 2000)
    {
        _delay = delay;
    }

public:
    void setup()
    {
        _last = 0;
    }

    void loop() override
    {
        if (millis() - _last > _delay)
        {
            if (_last == 0)
                first_update();
            else
                update();
            _last = millis();
        }
    }
};