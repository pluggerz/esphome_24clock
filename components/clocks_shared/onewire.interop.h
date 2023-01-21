#pragma once

#include "Arduino.h"

constexpr int CRC_BITS = 1;
constexpr int SRC_BITS = 6;  // 24 handles, so 2^5 = 32 should fit
constexpr int CMD_BITS = 4;

constexpr int RESERVED_BITS = 32 - SRC_BITS - CMD_BITS;

constexpr int SRC_MASTER = (1 << SRC_BITS) - 1;
constexpr int SRC_UNKNOWN = (1 << SRC_BITS) - 2;

namespace onewire {
int my_performer_id();

// to be placed in proper .h file
enum CmdEnum {
  FAULTY,

  DIRECTOR_ONLINE,  // All slaves should act if they came online
  PERFORMER_ONLINE,
  DIRECTOR_ACCEPT,
  DIRECTOR_PING,
  TOCK,
  PERFORMER_POSITION,
  PERFORMER_PREPPING,
  PERFORMER_CHECK_POINT,
  DIRECTOR_POSITION_ACK,
  REALIGN,
  DUMP_PERFORMERS
};

#ifdef ESP8266
constexpr int MAX_SCRATCH_LENGTH = 200;
extern char scratch_buffer[MAX_SCRATCH_LENGTH];
#endif

union OneCommand {
  uint32_t raw;

#ifdef ESP8266
  const char *format() {
    char const *from;
    int id = 0;
    if (this->from_master()) {
      from = "D";
      id = 0;
    } else if (this->from_performer()) {
      from = "p";
      id = this->msg.source_id;
    } else {
      from = "U";
      id = this->msg.source_id;
    }

    char const *what;
    switch (this->msg.cmd) {
      case CmdEnum::DIRECTOR_ONLINE:
        what = "DIRECTOR_ONLINE";
        break;
      case CmdEnum::PERFORMER_ONLINE:
        what = "PERFORMER_ONLINE";
        break;
      case onewire::TOCK:
        what = "TOCK";
        break;
      case onewire::REALIGN:
        what = "REALIGN";
        break;
      case CmdEnum::DIRECTOR_ACCEPT:
        what = "DIRECTOR_ACCEPT";
        break;
      case CmdEnum::PERFORMER_POSITION:
        what = "PERFORMER_POSITION";
        break;
      case CmdEnum::DIRECTOR_PING:
        what = "DIRECTOR_PING";
        break;
      case CmdEnum::PERFORMER_CHECK_POINT:
        what = "PERFORMER_CHECK_POINT";
        break;
      case CmdEnum::DIRECTOR_POSITION_ACK:
        what = "DIRECTOR_POSITION_ACK";
        break;
      case CmdEnum::DUMP_PERFORMERS:
        what = "DUMP_PERFORMERS";
        break;

      default:
        what = "UNKNOWN";
        break;
    }
    const char *parity = parity_okay() ? "Poke" : "P_FAILED!";
    switch (this->msg.cmd) {
      case CmdEnum::DIRECTOR_ONLINE:
        snprintf(scratch_buffer, MAX_SCRATCH_LENGTH, "[%s]%s%d->[%d]%s guid=%d",
                 parity, from, id, this->msg.cmd, what, this->msg.reserved);
        break;

      case CmdEnum::PERFORMER_CHECK_POINT:
        snprintf(scratch_buffer, MAX_SCRATCH_LENGTH,
                 "[%s]%s%d->[%d]%s check=%c value=%d", parity, from, id,
                 this->msg.cmd, what, this->check_point.id,
                 this->check_point.value);
        break;

      case CmdEnum::PERFORMER_POSITION:
        snprintf(scratch_buffer, MAX_SCRATCH_LENGTH,
                 "[%s]%s%d->[%d]%s ticks0=%d ticks1=%d", parity, from, id,
                 this->msg.cmd, what, this->position.ticks0,
                 this->position.ticks1);
        break;

      case CmdEnum::DIRECTOR_ACCEPT:
        snprintf(scratch_buffer, MAX_SCRATCH_LENGTH, "[%s]%s%d->[%d]%s baud=%d",
                 parity, from, id, this->msg.cmd, what, this->accept.baudrate);
        break;

      case CmdEnum::DIRECTOR_POSITION_ACK:
      case CmdEnum::DIRECTOR_PING:
      case CmdEnum::TOCK:
        snprintf(scratch_buffer, MAX_SCRATCH_LENGTH,
                 "[%s]%s%d->[%d]%s reserved=%d", parity, from, id,
                 this->msg.cmd, what, this->msg.reserved);
        break;

      default:
        snprintf(scratch_buffer, MAX_SCRATCH_LENGTH,
                 "[%s]%s%d->[%d]%s reserved=%d", parity, from, id,
                 this->msg.cmd, what, this->msg.reserved);
        break;
    }
    return scratch_buffer;
  }
#endif

#define SOURCE_HEADER      \
  uint32_t crc : CRC_BITS; \
  uint32_t cmd : CMD_BITS; \
  uint32_t source_id : SRC_BITS;

  OneCommand &fix_parity() {
    msg.crc = __builtin_parityl(raw >> 1);
    return *this;
  }

  bool parity_okay() { return __builtin_parityl(raw >> 1) == msg.crc; }

  struct Msg {
    SOURCE_HEADER;
    uint32_t reserved : RESERVED_BITS;

    static OneCommand by_director(CmdEnum cmd) {
      OneCommand ret;
      ret.raw = random();
      ret.msg.source_id = SRC_MASTER;
      ret.msg.cmd = cmd;
      return ret.fix_parity();
    };

    static OneCommand by_performer(CmdEnum cmd) {
      OneCommand ret;
      ret.raw = random();
      ret.msg.source_id = onewire::my_performer_id();
      ret.msg.cmd = cmd;
      return ret.fix_parity();
    };
  } __attribute__((packed, aligned(1))) msg;

  struct Accept {
    SOURCE_HEADER;
    // up to 256000 baud
    uint32_t baudrate : 18;
    uint32_t reserved : RESERVED_BITS - 18;
  } __attribute__((packed, aligned(1))) accept;

  struct Position {
    SOURCE_HEADER;
    constexpr static int POS_BITS = 10;

    uint32_t ticks0 : POS_BITS;
    uint32_t ticks1 : POS_BITS;
    uint32_t remainder : RESERVED_BITS - POS_BITS - POS_BITS;

    static OneCommand create(int handle0, int handle1) {
      OneCommand ret = Msg::by_performer(CmdEnum::PERFORMER_POSITION);
      ret.position.ticks0 = handle0;
      ret.position.ticks1 = handle1;
      return ret.fix_parity();
    }
  } __attribute__((packed, aligned(1))) position;

  struct CheckPoint {
    SOURCE_HEADER;
    constexpr static int CHECK_POINT_ID_BITS = 8;
    constexpr static int CHECK_POINT_BITS = 12;

    uint32_t id : CHECK_POINT_ID_BITS;
    uint32_t value : CHECK_POINT_BITS;
    uint32_t debug : 1;
    uint32_t remainder : RESERVED_BITS - CHECK_POINT_ID_BITS -
                         CHECK_POINT_BITS - 1;

    static OneCommand create(bool debug, uint8_t id, uint32_t value) {
      OneCommand ret = Msg::by_performer(CmdEnum::PERFORMER_CHECK_POINT);
      ret.check_point.debug = debug ? 1 : 0;
      ret.check_point.id = id;
      ret.check_point.value = value;
      return ret.fix_parity();
    }

    static OneCommand for_info(uint8_t id, uint32_t value) {
      return create(false, id, value);
    }

    static OneCommand for_debug(uint8_t id, uint32_t value) {
      return create(true, id, value);
    }
  } __attribute__((packed, aligned(1))) check_point;

  static OneCommand performer_online() {
    OneCommand cmd;
    cmd.raw = random();
    cmd.msg.source_id = SRC_UNKNOWN;
    cmd.msg.cmd = CmdEnum::PERFORMER_ONLINE;
    return cmd.fix_parity();
  }

  static OneCommand tock(int performer_id, uint8_t tick_id) {
    OneCommand cmd;
    cmd.raw = random();
    cmd.msg.source_id = performer_id;
    cmd.msg.cmd = CmdEnum::TOCK;
    cmd.msg.reserved = tick_id;
    return cmd.fix_parity();
  }

  bool from_master() const { return msg.source_id == SRC_MASTER; }

  bool from_performer() const {
    return !from_master() && msg.source_id != SRC_UNKNOWN;
  }

  int derive_next_source_id() const {
    if (from_master())
      return 0;
    else if (from_performer())
      return msg.source_id + 1;
    return SRC_UNKNOWN;
  }

  OneCommand increase_performer_id_and_forward() const {
    OneCommand ret;
    ret.raw = raw;
    ret.msg.source_id = derive_next_source_id();
    return ret.fix_parity();
  }
} __attribute__((packed, aligned(1)));

class CommandBuilder {
 public:
  OneCommand accept(uint32_t baudrate) {
    OneCommand ret = OneCommand::Msg::by_director(CmdEnum::DIRECTOR_ACCEPT);
    ret.accept.baudrate = baudrate;
    ret.accept.reserved = random();
    return ret.fix_parity();
  }

  OneCommand ping() {
    OneCommand cmd;
    cmd.raw = random();
    cmd.msg.source_id = SRC_MASTER;
    cmd.msg.cmd = CmdEnum::DIRECTOR_PING;
    return cmd.fix_parity();
  }

  OneCommand realign() {
    OneCommand cmd;
    cmd.raw = random();
    cmd.msg.source_id = SRC_MASTER;
    cmd.msg.cmd = CmdEnum::REALIGN;
    return cmd.fix_parity();
  }

  OneCommand director_online(int guid) {
    OneCommand ret = OneCommand::Msg::by_director(CmdEnum::DIRECTOR_ONLINE);
    ret.msg.reserved = guid;
    return ret.fix_parity();
  }
  OneCommand dump_performers_by_director() {
    return OneCommand::Msg::by_director(CmdEnum::DUMP_PERFORMERS);
  }
  OneCommand dump_performers_by_performer() {
    return OneCommand::Msg::by_performer(CmdEnum::DUMP_PERFORMERS);
  }
  OneCommand director_position_ack(int guid) {
    OneCommand ret =
        OneCommand::Msg::by_director(CmdEnum::DIRECTOR_POSITION_ACK);
    ret.msg.reserved = guid;
    return ret.fix_parity();
  }
} extern command_builder;
}  // namespace onewire
