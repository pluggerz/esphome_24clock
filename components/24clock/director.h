#include "stub.h"
#if defined(MASTER) && !defined(MASTER_H)
#define MASTER_H

#include "esphome/core/component.h"
#include "esphome/components/time/real_time_clock.h"

#include "onewire.h"

namespace clock24
{
    class Director : public esphome::Component
    {
    private:
        esphome::time::RealTimeClock *time_;
        bool dumped = false;
        int _performers = -1;
        bool _killed = false;

    protected:
        virtual void dump_config() override;
        virtual void setup() override;
        virtual void loop() override;

    public:
        Director();

        void kill();

        void set_time(esphome::time::RealTimeClock *value)
        {
            time_ = value;
        }
    };
}
#endif