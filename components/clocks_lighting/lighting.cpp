
#include "lighting.h"

using lighting::LightingController;

void LightingController::set_director(Director *director) {
  this->director = director;
  director->add_listener(this);
}
