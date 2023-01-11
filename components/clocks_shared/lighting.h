#pragma once

namespace lighting {
enum LightingMode {
  // NOTE, do not change the ids below!
  WarmWhiteShimmer = 0,
  RandomColorWalk = 1,
  TraditionalColors = 2,
  ColorExplosion = 3,
  Gradient = 4,
  BrightTwinkle = 5,
  Collision = 6,
  // NOTE, do not change the ids above!
  Off = 7,
  Solid = 8,
  Rainbow = 9,
  Debug = 10,
};
}