#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace kafka {

    // Топик, через который чекер сообщает боту о смене статуса сайта. Ключ сообщения — URL:
    // события одного сайта попадают в одну партицию и читаются в порядке записи.
    inline constexpr const char *SITE_STATUS_TOPIC = "site-status";

    // Контракт между процессами DevOpsBot и TelegramBot. Меняешь поля — синхронно правь
    // ToJson/FromJson: старый бот должен понимать новые события, и наоборот.
    struct SiteStatusEvent {
        std::string url;
        bool is_up = true;
        int http_code = 0;
        // Когда статус изменился, unix-время в секундах.
        std::int64_t changed_at = 0;

        bool operator==(const SiteStatusEvent &) const = default;
    };

    [[nodiscard]] std::string ToJson(const SiteStatusEvent &event);

    // nullopt — не JSON, не объект или нет нужных полей: обработать такое сообщение нельзя.
    [[nodiscard]] std::optional<SiteStatusEvent> FromJson(std::string_view json);

} // namespace kafka
