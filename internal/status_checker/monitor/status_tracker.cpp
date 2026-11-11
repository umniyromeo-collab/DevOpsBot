#include "status_tracker.h"

namespace monitor {

    StatusUpdate ApplyCheck(const SiteStatus &current, bool check_is_up, int http_code) {
        StatusUpdate update{current, false};
        update.status.last_http_code = http_code;

        if (check_is_up) {
            // Для восстановления хватает одной удачи: о том, что сайт снова работает,
            // лучше узнать сразу.
            update.status.consecutive_failures = 0;
            if (!current.is_up) {
                update.status.is_up = true;
                update.changed = true;
            }
            return update;
        }

        update.status.consecutive_failures = current.consecutive_failures + 1;
        if (current.is_up && update.status.consecutive_failures >= FAILURES_TO_DOWN) {
            update.status.is_up = false;
            update.changed = true;
        }
        return update;
    }

} // namespace monitor
