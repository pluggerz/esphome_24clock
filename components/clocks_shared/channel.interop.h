#pragma once

#include "Arduino.h"

namespace channel {
enum MsgEnum {
  MSG_TICK,
  MSG_PERFORMER_SETTINGS,
  MSG_POSITION_REQUEST,
  MSG_BEGIN_KEYS,
  MSG_SEND_KEYS,
  MSG_END_KEYS,
  MSG_LIGTHING_MODE
};

constexpr int MAX_UART_MESSAGE_SIZE = 32;

struct ChannelInterop {
  constexpr static uint8_t DIRECTOR = 0xFF;
  constexpr static uint8_t ALL_PERFORMERS = 32;
  constexpr static uint8_t UNDEFINED = 0xFF - 1;

  // NOTE: should be declared by director/performer
  static uint8_t id;

  /**
   * @brief Get the my id object
   *
   * @return uint8_t
   */
  static uint8_t get_my_id() { return id; }
};

struct Message {
 protected:
  uint8_t source_id;
  uint8_t msgType;
  uint8_t destination_id;

 public:
  int getSourceId() const { return source_id; }
  MsgEnum getMsgEnum() const { return (MsgEnum)msgType; }
  int getDstId() const { return destination_id; }

  Message(uint8_t source_id, MsgEnum msgType,
          uint8_t destination_id = ChannelInterop::ALL_PERFORMERS)
      : source_id(source_id),
        msgType((uint8_t)msgType),
        destination_id(destination_id) {}
} __attribute__((packed, aligned(1)));

namespace messages {
struct IntMessage : public Message {
 public:
  int32_t value;

  IntMessage(int32_t value)
      : Message(-1, MsgEnum::MSG_TICK, ChannelInterop::ALL_PERFORMERS),
        value(value) {}
} __attribute__((packed, aligned(1)));

#if defined(IS_DIRECTOR)
static IntMessage tick(uint8_t tick_id) { return IntMessage(tick_id); }

static Message request_positions() {
  return Message(-1, MsgEnum::MSG_POSITION_REQUEST);
}
#endif

struct StepperSettings : public Message {
 public:
  int16_t magnet_offet0, magnet_offet1;

  StepperSettings(uint8_t destination_id, int16_t magnet_offet0,
                  int16_t magnet_offet1)
      : Message(-1, MsgEnum::MSG_PERFORMER_SETTINGS, destination_id),
        magnet_offet0(magnet_offet0),
        magnet_offet1(magnet_offet1) {}
} __attribute__((packed, aligned(1)));

#define MAX_ANIMATION_KEYS 90
#define MAX_ANIMATION_KEYS_PER_MESSAGE 14  // MAX 14!

struct UartKeysMessage : public Message {
  // it would be tempting to use: Cmd cmds[MAX_ANIMATION_KEYS_PER_MESSAGE] ={};
  // but padding is messed up between esp and uno :() note we control the bits
  // better ;P
 private:
  uint8_t _size;
  uint16_t cmds[MAX_ANIMATION_KEYS_PER_MESSAGE] = {};

 public:
  UartKeysMessage(uint8_t destination_id, uint8_t _size)
      : Message(-1, MsgEnum::MSG_SEND_KEYS, destination_id), _size(_size) {}
  uint8_t size() const { return _size; }

  void set_key(int idx, uint16_t value) { cmds[idx] = value; }

  uint16_t get_key(int idx) const { return cmds[idx]; }
} __attribute__((packed, aligned(1)));

/***
 *
 * Essentially every minute we send animation keys.
 * Some type of animations depends on the seconds.
 * Ideally, a minute has 60 seconds, but some time could be lost in prepping.
 * So, this message will give the actual minute.
 * Or at least the time when the master will probability send the next
 * instructions.
 */
struct UartEndKeysMessage : public Message {
 public:
  uint32_t number_of_millis_left;
  uint8_t turn_speed, turn_steps;
  uint8_t speed_map[8];
  uint64_t speed_detection;

  UartEndKeysMessage(const uint8_t turn_speed, const uint8_t turn_steps,
                     const uint8_t (&speed_map)[8], uint64_t _speed_detection,
                     uint32_t number_of_millis_left)
      : Message(-1, MsgEnum::MSG_END_KEYS, ChannelInterop::ALL_PERFORMERS),
        number_of_millis_left(number_of_millis_left),
        turn_speed(turn_speed),
        turn_steps(turn_steps),
        speed_detection(_speed_detection) {
    for (int idx = 0; idx < 8; ++idx) {
      this->speed_map[idx] = speed_map[idx];
    }
  }
} __attribute__((packed, aligned(1)));

struct LightingMode : public Message {
 public:
  const uint8_t mode;
  uint8_t r = 0, g = 0, b = 0xFF;
  LightingMode(uint8_t mode)
      : Message(-1, MsgEnum::MSG_LIGTHING_MODE, ChannelInterop::ALL_PERFORMERS),
        mode(mode) {}
} __attribute__((packed, aligned(1)));

}  // namespace messages
}  // namespace channel
