#include "priority.h"

#include <algorithm>

namespace monitor {

    std::chrono::seconds EffectiveInterval(std::chrono::seconds base, int recent_failures) {
        if (recent_failures <= 0) {
            return base;
        }
        const std::chrono::seconds faster =
            std::max<std::chrono::seconds>(MIN_INTERVAL, base / FAILURE_SPEEDUP);
        // Если базовый интервал сам меньше нижней границы, не замедляем опрос.
        return std::min(base, faster);
    }

} // namespace monitor
