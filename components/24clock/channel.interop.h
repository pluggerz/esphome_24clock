#include "Arduino.h"

enum ChannelMsgEnum
{
    MSG_TICK,
};

const int MAX_UART_MESSAGE_SIZE = 32;
const int ALL_SLAVES = 32;

struct ChannelMessage
{
private:
    uint8_t source_id;
    uint8_t msgType;
    uint8_t destination_id;

public:
    int getSourceId() const { return source_id; }
    ChannelMsgEnum getMsgEnum() const { return (ChannelMsgEnum)msgType; }
    int getDstId() const { return destination_id; }

    ChannelMessage(uint8_t source_id, ChannelMsgEnum msgType) : source_id(source_id), msgType((u8)msgType), destination_id(ALL_SLAVES) {}
    ChannelMessage(uint8_t source_id, ChannelMsgEnum msgType, uint8_t destination_id) : source_id(source_id), msgType((uint8_t)msgType), destination_id(destination_id) {}

    static ChannelMessage tick()
    {
        return ChannelMessage(-1, ChannelMsgEnum::MSG_TICK);
    }
} __attribute__((packed, aligned(1)));
