#pragma once

#include <chrono>
#include <string_view>

namespace telegram {

    enum class SendOutcome {
        Sent,  // Telegram принял сообщение
        Retry, // временная ошибка: сеть, 5xx, ограничение частоты — повторить то же сообщение
        Skip, // окончательный отказ (например, пользователь заблокировал бота) — получателя
              // пропустить
    };

    struct SendResult {
        SendOutcome outcome = SendOutcome::Retry;
        // Сколько ждать перед повтором по просьбе Telegram (429); 0 — решает вызывающий.
        std::chrono::seconds retry_after{0};
    };

    // Классифицирует ответ sendMessage. status_code == 0 — ответа не было (сеть, таймаут).
    [[nodiscard]] SendResult ClassifySendResponse(int status_code, std::string_view body);

} // namespace telegram
