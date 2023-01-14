#include "leds.background.h"

rgb_color BackgroundLayer::colors[LED_COUNT];

#ifdef USE_LED_BUFFER
rgb_color BackgroundLayer::buffer_colors[LED_COUNT];
#endif

// keep empty, just testing if the header can live without any other