#include "site_status_event.h"

#include <nlohmann/json.hpp>

namespace kafka {

    std::string ToJson(const SiteStatusEvent &event) {
        return nlohmann::json{
            {"url", event.url},
            {"is_up", event.is_up},
            {"http_code", event.http_code},
            {"changed_at", event.changed_at},
        }
            .dump();
    }

    std::optional<SiteStatusEvent> FromJson(std::string_view json) {
        // Без исключений на разборе: битое сообщение в топике — не повод ронять бота.
        const auto parsed = nlohmann::json::parse(json, nullptr, /*allow_exceptions=*/false);
        if (parsed.is_discarded() || !parsed.is_object()) {
            return std::nullopt;
        }
        try {
            SiteStatusEvent event;
            event.url = parsed.at("url").get<std::string>();
            event.is_up = parsed.at("is_up").get<bool>();
            event.http_code = parsed.at("http_code").get<int>();
            event.changed_at = parsed.at("changed_at").get<std::int64_t>();
            return event;
        } catch (const nlohmann::json::exception &) {
            // Нет поля или поле не того типа.
            return std::nullopt;
        }
    }

} // namespace kafka
