#pragma once

#include "send_result.h"

#include <cstdint>
#include <string>

namespace telegram {

    // Отправка сообщения в чат. Шов между доставкой уведомлений и Telegram: тесты подменяют
    // его фейком, в работе — telegram::Bot.
    class IMessageSender {
    public:
        virtual ~IMessageSender() = default;

        [[nodiscard]] virtual SendResult Send(std::int64_t chat_id, const std::string &text) = 0;
    };

} // namespace telegram
