#pragma once

#include "Arduino.h"

namespace channel {
enum MsgEnum {
  MSG_TICK,
  MSG_PERFORMER_SETTINGS,
  MSG_REQUEST_POSITIONS,
  MSG_BEGIN_KEYS,
  MSG_SEND_KEYS,
  MSG_END_KEYS,
  MSG_KILL_KEYS_OR_REQUEST_POSITIONS,
  MSG_LIGTHING_MODE,
  MSG_INDIVIDUAL_LEDS_SET,
  MSG_INDIVIDUAL_LEDS_SHOW,
  MSG_DUMP_PERFORMERS_VIA_CHANNEL,
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
struct TickMessage : public Message {
 public:
  int32_t value;

  TickMessage(int32_t value)
      : Message(-1, MsgEnum::MSG_TICK, ChannelInterop::ALL_PERFORMERS),
        value(value) {}
} __attribute__((packed, aligned(1)));

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
  uint8_t r = 0, g = 0, b = 0xFF, brightness = 31;
  LightingMode(uint8_t mode, uint8_t r, uint8_t g, uint8_t b,
               uint8_t brightness)
      : Message(-1, MsgEnum::MSG_LIGTHING_MODE, ChannelInterop::ALL_PERFORMERS),
        mode(mode),
        r(r),
        g(g),
        b(b),
        brightness(brightness) {}
} __attribute__((packed, aligned(1)));

struct IndividualLeds : public Message {
 public:
  struct Rgb {
    uint16_t r : 5, g : 6, b : 5;
  } __attribute__((packed, aligned(1)));
  Rgb leds[12];

  IndividualLeds(int performer_id)
      : Message(-1, MsgEnum::MSG_INDIVIDUAL_LEDS_SET, performer_id) {}
} __attribute__((packed, aligned(1)));

struct RequestPositions : public Message {
 public:
  uint8_t puid;

  RequestPositions(MsgEnum msg, uint8_t puid) : Message(-1, msg), puid(puid) {}
} __attribute__((packed, aligned(1)));

}  // namespace messages

class MessageBuilder {
 public:
  messages::RequestPositions request_positions(uint8_t puid) {
    return messages::RequestPositions(MsgEnum::MSG_REQUEST_POSITIONS, puid);
  }

  messages::RequestPositions request_kill_keys_or_request_position(
      uint8_t puid) {
    return messages::RequestPositions(
        MsgEnum::MSG_KILL_KEYS_OR_REQUEST_POSITIONS, puid);
  }

  Message dump_performers_by_director() {
    return Message(-1, MsgEnum::MSG_DUMP_PERFORMERS_VIA_CHANNEL);
  }

  messages::TickMessage tick(uint8_t tick_id) {
    return messages::TickMessage(tick_id);
  }
} extern message_builder;

}  // namespace channel
