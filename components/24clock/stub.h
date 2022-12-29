#pragma once

#define MODE_ONEWIRE_PASSTROUGH 1
#define MODE_ONEWIRE_MIRROR 2
#define MODE_ONEWIRE_VALUE 3

#define MODE_ONEWIRE_CMD 4

// make sure all slaves+master use this one as well
#define MODE_CHANNEL 6

// MODE_ONEWIRE_INTERACT can work with PASSTHROUGH and MIRROR for slaves
#define MODE_ONEWIRE_INTERACT 7

#define MODE MODE_ONEWIRE_INTERACT

typedef unsigned long Micros;
typedef unsigned long Millis;

constexpr int NMBR_OF_PERFORMERS = 24;

#ifdef ESP8266
#define MASTER
#define MASTER

#include "stub.master.h"

#else
#define SLAVE
#define PERFORMER

#include "slave/stub.slave.h"

#endif

constexpr int SRC_BITS = 6; // 24 handles, so 2^5 = 32 should fit
constexpr int CMD_BITS = 3;

constexpr int RESERVED_BITS = 32 - SRC_BITS - CMD_BITS;

constexpr int SRC_MASTER = (1 << SRC_BITS) - 1;
constexpr int SRC_UNKNOWN = (1 << SRC_BITS) - 2;

enum CmdEnum
{
    FAULTY,

    DIRECTOR_ONLINE, // All slaves should act if they came online
    PERFORMER_ONLINE,
    DIRECTOR_ACCEPT,
    DIRECTOR_PING,
    TOCK,
    PERFORMER_POSITION,
    PERFORMER_PREPPING,
};

#ifdef ESP8266
#define IS_DIRECTOR
#else
#define IS_PERFORMER
#endif

// to be placed in proper .h file
union OneCommand
{
    uint32_t raw;

#define SOURCE_HEADER        \
    uint32_t cmd : CMD_BITS; \
    uint32_t source_id : SRC_BITS;

    struct Msg
    {
        SOURCE_HEADER;
        uint32_t reserved : RESERVED_BITS;

        static OneCommand by_director(CmdEnum cmd)
        {
            OneCommand ret;
            ret.msg.source_id = SRC_MASTER;
            ret.msg.cmd = cmd;
            return ret;
        };

        static OneCommand by_performer(CmdEnum cmd, int performer_id)
        {
            OneCommand ret;
            ret.msg.source_id = performer_id;
            ret.msg.cmd = cmd;
            return ret;
        };
    } __attribute__((packed, aligned(1))) msg;

    struct Accept
    {
        SOURCE_HEADER;
        uint32_t baudrate : RESERVED_BITS;

        static OneCommand create(uint32_t baudrate)
        {
            OneCommand ret = Msg::by_director(CmdEnum::DIRECTOR_ACCEPT);
            ret.accept.baudrate = baudrate;
            return ret;
        }
    } __attribute__((packed, aligned(1))) accept;

    struct Position
    {
        SOURCE_HEADER;
        constexpr static int POS_BITS = 10;

        uint32_t pos0 : POS_BITS;
        uint32_t pos1 : POS_BITS;
        uint32_t remainder : RESERVED_BITS - POS_BITS - POS_BITS;

        static OneCommand create(int pos0, int pos1)
        {
            OneCommand ret = Msg::by_performer(CmdEnum::PERFORMER_POSITION, -1);
            ret.position.pos0 = pos0;
            ret.position.pos1 = pos1;
            return ret;
        }
    } __attribute__((packed, aligned(1))) position;

    static OneCommand director_online(int guid)
    {
        OneCommand ret = Msg::by_director(CmdEnum::DIRECTOR_ONLINE);
        ret.msg.reserved = guid;
        return ret;
    }

    static OneCommand performer_online();
    static OneCommand ping();
    static OneCommand tock(int performer_id);

    bool from_master() const
    {
        return msg.source_id == SRC_MASTER;
    }

    bool from_performer() const
    {
        return !from_master() && msg.source_id != SRC_UNKNOWN;
    }

    int derive_next_source_id() const
    {
        if (from_master())
            return 0;
        else if (from_performer())
            return msg.source_id + 1;
        return SRC_UNKNOWN;
    }

    OneCommand forward() const
    {
        OneCommand ret;
        ret.raw = raw;
        ret.msg.source_id = derive_next_source_id();
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