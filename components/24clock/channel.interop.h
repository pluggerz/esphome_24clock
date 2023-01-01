#include "Arduino.h"

namespace channel {
enum MsgEnum {
  MSG_TICK,
  MSG_PERFORMER_SETTINGS,
  MSG_POSITION_REQUEST,
  MSG_BEGIN_KEYS,
  MSG_SEND_KEYS,
  MSG_END_KEYS
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

static IntMessage tick(uint8_t tick_id) { return IntMessage(tick_id); }

static Message request_positions() {
  return Message(-1, MsgEnum::MSG_POSITION_REQUEST);
}

struct StepperSettings : public Message {
 public:
  int16_t magnet_offet0, magnet_offet1;

  StepperSettings(uint8_t destination_id, int16_t magnet_offet0,
                  int16_t magnet_offet1)
      : Message(-1, MsgEnum::MSG_PERFORMER_SETTINGS, destination_id),
        magnet_offet0(magnet_offet0),
        magnet_offet1(magnet_offet1) {}
} __attribute__((packed, aligned(1)));

}  // namespace messages
}  // namespace channel