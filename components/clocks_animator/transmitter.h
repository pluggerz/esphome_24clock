#include "../clocks_director/animation.h"
#include "../clocks_director/keys.h"
#include "../clocks_shared/channel.h"

namespace transmitter {
using keys::DeflatedCmdKey;
using rs485::BufferChannel;

class Transmitter {
 public:
  AnimationController *animation_controller;
  rs485::BufferChannel *channel;

  Transmitter(AnimationController *animation_controller, BufferChannel *channel)
      : animation_controller(animation_controller), channel(channel) {}

  void sendCommandsForHandle(int animatorHandleId,
                             const std::vector<keys::DeflatedCmdKey> &commands);

  void sendAndClear(int handleId, std::vector<DeflatedCmdKey> &selected) {
    if (selected.size() > 0) {
      sendCommandsForHandle(handleId, selected);
      selected.clear();
    }
  }

  void sendCommands(std::vector<HandleCmd> &cmds);
  void sendInstructions(Instructions &instructions, u32 millisLeft = u32(-1));
};
}  // namespace transmitter