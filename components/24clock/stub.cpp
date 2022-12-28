#include "stub.h"

#include "onewire.h"
#include "pins.h"

OneCommand OneCommand::online()
{
    OneCommand cmd;
    cmd.msg.src = SRC_UNKNOWN;
    cmd.msg.cmd = CmdEnum::PERFORMER_ONLINE;
    return cmd;
}

OneCommand OneCommand::ping()
{
    OneCommand cmd;
    cmd.msg.src = SRC_MASTER;
    cmd.msg.cmd = CmdEnum::DIRECTOR_PING;
    return cmd;
}

OneCommand OneCommand::tock(int performer_id)
{
    OneCommand cmd;
    cmd.msg.src = performer_id;
    cmd.msg.cmd = CmdEnum::TOCK;
    return cmd;
}