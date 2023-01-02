#include <functional>

#include "../clocks_director/animation.h"

namespace animator24 {

class DistanceCalculators {
 public:
  typedef std::function<int(int from, int to)> Func;

  static Func shortest;
  static Func clockwise;
  static Func antiClockwise;

  static Func random() {
    std::vector<Func> options = {shortest, clockwise, antiClockwise};
    return options[::random(options.size())];
  }
};

class HandlesAnimations {
 public:
  typedef std::function<void(Instructions &instructions, int speed,
                             const HandlesState &goal,
                             const DistanceCalculators::Func &steps_calculator)>
      Func;

  static void instruct_using_swipe(
      Instructions &instructions, int speed, const HandlesState &goal,
      const DistanceCalculators::Func &steps_calculator);
  static void instructUsingStepCalculator(
      Instructions &instructions, int speed, const HandlesState &goal,
      const DistanceCalculators::Func &steps_calculator);

  static void instruct_using_random(
      Instructions &instructions, int speed, const HandlesState &goal,
      const DistanceCalculators::Func &steps_calculator) {
    std::vector<Func> options = {instruct_using_swipe,
                                 instructUsingStepCalculator};
    options[::random(options.size())](instructions, speed, goal,
                                      steps_calculator);
  }
};

class InBetweenAnimations {
 public:
  typedef std::function<void(Instructions &instructions, int speed)> Func;

  static void instructDelayUntilAllAreReady(Instructions &instructions,
                                            int speed,
                                            double additional_time = 0);

  static void instructNone(Instructions &instructions, int speed) { return; }

  static void instructStarAnimation(Instructions &instructions, int speed);
  static void instructDashAnimation(Instructions &instructions, int speed);
  static void instructMiddlePointAnimation(Instructions &instructions,
                                           int speed);
  static void instructAllInnerPointAnimation(Instructions &instructions,
                                             int speed);
  static void instructPacManAnimation(Instructions &instructions, int speed);

  static void instructRandom(Instructions &instructions, int speed) {
    std::vector<Func> options = {
        instructDashAnimation, instructMiddlePointAnimation,
        instructAllInnerPointAnimation, instructStarAnimation,
        instructPacManAnimation};
    options[::random(options.size())](instructions, speed);
  }
};

}  // namespace animator24