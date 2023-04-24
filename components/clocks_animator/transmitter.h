#include "../clocks_director/animation.h"
#include "../clocks_performer/keys.h"

namespace transmitter {
using keys::DeflatedCmdKey;

class Transmitter {
 public:
  AnimationController *animation_controller;

  Transmitter(AnimationController *animation_controller)
      : animation_controller(animation_controller) {}

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