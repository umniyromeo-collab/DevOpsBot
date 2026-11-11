#pragma once

#include "site_status.h"

namespace monitor {

    // Сколько неудач подряд превращают работающий сайт в упавший. Одна неудача — часто случайный
    // сетевой сбой, будить из-за неё пользователя не стоит.
    inline constexpr int FAILURES_TO_DOWN = 2;

    struct StatusUpdate {
        SiteStatus status;
        // is_up изменился: сайт упал или восстановился.
        bool changed = false;
    };

    // Применяет результат проверки к текущему статусу. Чистая функция без I/O и часов, поэтому
    // все правила проверяются тестами напрямую. changed_at и notified_is_up не трогает:
    // первое выставляет вызывающий при changed == true, второе меняется только после ответа Kafka.
    [[nodiscard]] StatusUpdate ApplyCheck(const SiteStatus &current, bool check_is_up,
                                          int http_code);

} // namespace monitor
