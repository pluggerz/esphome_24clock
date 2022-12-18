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

OneCommand OneCommand::accept()
{
    OneCommand cmd;
    cmd.msg.src = SRC_MASTER;
    cmd.msg.cmd = CmdEnum::DIRECTOR_ACCEPT;
    return cmd;
}

OneCommand OneCommand::ping()
{
    OneCommand cmd;
    cmd.msg.src = SRC_MASTER;
    cmd.msg.cmd = CmdEnum::DIRECTOR_PING;
    return cmd;
}