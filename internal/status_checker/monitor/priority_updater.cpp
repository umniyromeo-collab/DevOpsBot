#include "priority_updater.h"

#include "priority.h"

namespace monitor {

    bool PriorityUpdater::MaybeUpdate(Scheduler::Clock::time_point now, Scheduler &scheduler) {
        if (now < next_) {
            return false;
        }
        scheduler.UpdateIntervals(store_.RecentFailures(FAILURE_WINDOW));
        next_ = now + RECALC_PERIOD;
        return true;
    }

} // namespace monitor
