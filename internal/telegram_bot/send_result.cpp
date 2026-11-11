#include "send_result.h"

#include <nlohmann/json.hpp>

namespace telegram {

    SendResult ClassifySendResponse(int status_code, std::string_view body) {
        if (status_code == 200) {
            return {SendOutcome::Sent};
        }
        if (status_code == 429) {
            // Telegram сам говорит, сколько ждать: {"parameters": {"retry_after": N}}.
            SendResult result{SendOutcome::Retry};
            const auto parsed = nlohmann::json::parse(body, nullptr, /*allow_exceptions=*/false);
            if (!parsed.is_discarded() && parsed.is_object() && parsed.contains("parameters") &&
                parsed["parameters"].is_object() && parsed["parameters"].contains("retry_after") &&
                parsed["parameters"]["retry_after"].is_number_integer()) {
                result.retry_after =
                    std::chrono::seconds{parsed["parameters"]["retry_after"].get<int>()};
            }
            return result;
        }
        if (status_code >= 400 && status_code < 500) {
            // 400 «chat not found», 403 «bot was blocked by the user»: повтор не поможет,
            // а зациклившись на одном получателе, бот перестал бы доставлять остальным.
            return {SendOutcome::Skip};
        }
        // Нет ответа, 5xx и всё непонятное — временно.
        return {SendOutcome::Retry};
    }

} // namespace telegram
