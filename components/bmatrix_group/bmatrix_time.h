#pragma once
#ifdef USE_TIME
#include "bmatrix_group.h"

namespace esphome {
namespace bmatrix {

using esphome::time::RealTimeClock;
typedef RealTimeClock* RealTimeClockPntr;

class TimeWidget : public TextBasedWidget {
 protected:
  const RealTimeClockPntr clock;

 public:
  TimeWidget(const RealTimeClockPntr clock) : clock(clock) {}
};

class DigitalTimeWidget : public TimeWidget {
 protected:
  const std::string format;

  virtual ScratchItem render(Scratch& parent_scratch) override {
    char buffer[64]{0};
    auto time = clock->now();
    int bytes = time.strftime(buffer, sizeof(buffer), format.c_str());
    return render_text(parent_scratch, buffer);
  }

 public:
  DigitalTimeWidget(const RealTimeClockPntr clock, const std::string& format)
      : TimeWidget(clock), format(format) {}
};

}  // namespace bmatrix
}  // namespace esphome
#endif