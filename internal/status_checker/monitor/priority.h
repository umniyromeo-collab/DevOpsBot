#pragma once

#include <chrono>

namespace monitor {

    // Окно истории, по которому решается, были ли у сайта недавние сбои.
    inline constexpr std::chrono::minutes FAILURE_WINDOW{15};
    // Как часто планировщик пересчитывает интервалы по истории.
    inline constexpr std::chrono::seconds RECALC_PERIOD{30};
    // Нижняя граница: опрашивать чаще таймаута фетчера (10 с) бессмысленно.
    inline constexpr std::chrono::seconds MIN_INTERVAL{10};
    // Во сколько раз учащается опрос сайта с недавними сбоями.
    inline constexpr int FAILURE_SPEEDUP = 4;

    // Интервал опроса сайта с базовым интервалом base и recent_failures неудачами в окне.
    // Сайт со сбоями опрашивается чаще — о восстановлении узнаем быстрее, — но не чаще
    // MIN_INTERVAL и никогда не реже базового интервала.
    [[nodiscard]] std::chrono::seconds EffectiveInterval(std::chrono::seconds base,
                                                         int recent_failures);

} // namespace monitor
