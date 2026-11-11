#pragma once

#include <cstdint>

namespace monitor {

    // Статус сайта — то же, что строка в таблице site_state.
    struct SiteStatus {
        // Начальный статус — «работает»: первая успешная проверка нового сайта не считается сменой.
        bool is_up = true;
        // Последний статус, приём которого подтвердила Kafka. Пока он отличается от is_up,
        // событие нужно дослать — так смена статуса не теряется при падении чекера.
        bool notified_is_up = true;
        int consecutive_failures = 0;
        // Когда статус менялся в последний раз, unix-время в секундах.
        std::int64_t changed_at = 0;
        // Код последней проверки: нужен событию, если его досылают после перезапуска.
        int last_http_code = 0;
    };

} // namespace monitor
