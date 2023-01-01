#pragma once

#include "Arduino.h"

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
};

union OneCommand {
  uint32_t raw;

#define SOURCE_HEADER      \
  uint32_t cmd : CMD_BITS; \
  uint32_t source_id : SRC_BITS;

  struct Msg {
    SOURCE_HEADER;
    uint32_t reserved : RESERVED_BITS;

    static OneCommand by_director(CmdEnum cmd) {
      OneCommand ret;
      ret.msg.source_id = SRC_MASTER;
      ret.msg.cmd = cmd;
      return ret;
    };

    static OneCommand by_performer(CmdEnum cmd, int performer_id) {
      OneCommand ret;
      ret.msg.source_id = performer_id;
      ret.msg.cmd = cmd;
      return ret;
    };
  } __attribute__((packed, aligned(1))) msg;

  struct Accept {
    SOURCE_HEADER;
    uint32_t baudrate : RESERVED_BITS;

    static OneCommand create(uint32_t baudrate) {
      OneCommand ret = Msg::by_director(CmdEnum::DIRECTOR_ACCEPT);
      ret.accept.baudrate = baudrate;
      return ret;
    }
  } __attribute__((packed, aligned(1))) accept;

  struct Position {
    SOURCE_HEADER;
    constexpr static int POS_BITS = 10;

    uint32_t ticks0 : POS_BITS;
    uint32_t ticks1 : POS_BITS;
    uint32_t remainder : RESERVED_BITS - POS_BITS - POS_BITS;

    static OneCommand create(int performer_id, int handle0, int handle1) {
      OneCommand ret =
          Msg::by_performer(CmdEnum::PERFORMER_POSITION, performer_id);
      ret.position.ticks0 = handle0;
      ret.position.ticks1 = handle1;
      return ret;
    }
  } __attribute__((packed, aligned(1))) position;

  struct CheckPoint {
    SOURCE_HEADER;
    constexpr static int CHECK_POINT_BITS = 8;

    uint32_t id : CHECK_POINT_BITS;
    uint32_t value : CHECK_POINT_BITS;
    uint32_t debug : 1;
    uint32_t remainder : RESERVED_BITS - 2 * CHECK_POINT_BITS - 1;

    static OneCommand create(bool debug, uint8_t id, uint8_t value) {
      OneCommand ret =
          Msg::by_performer(CmdEnum::PERFORMER_CHECK_POINT, my_performer_id());
      ret.check_point.debug = debug ? 1 : 0;
      ret.check_point.id = id;
      ret.check_point.value = value;
      return ret;
    }

    static OneCommand for_info(uint8_t id, uint8_t value) {
      return create(false, id, value);
    }

    static OneCommand for_debug(uint8_t id, uint8_t value) {
      return create(true, id, value);
    }
  } __attribute__((packed, aligned(1))) check_point;

  static OneCommand director_online(int guid) {
    OneCommand ret = Msg::by_director(CmdEnum::DIRECTOR_ONLINE);
    ret.msg.reserved = guid;
    return ret;
  }

  static OneCommand performer_online();
  static OneCommand ping();
  static OneCommand tock(int performer_id, uint8_t tick_id);

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

  OneCommand forward() const {
    OneCommand ret;
    ret.raw = raw;
    ret.msg.source_id = derive_next_source_id();
    return ret;
  }
} __attribute__((packed, aligned(1)));
}  // namespace onewire
