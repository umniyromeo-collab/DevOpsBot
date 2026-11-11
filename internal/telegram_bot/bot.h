#pragma once

#include "message_sender.h"

#include <cstdint>
#include <string>
#include <vector>

namespace telegram {

    // Входящее текстовое сообщение.
    struct IncomingMessage {
        std::int64_t chat_id = 0;
        std::string text;
    };

    // Клиент Telegram Bot API поверх cpr (libcurl): только транспорт, без логики команд.
    //
    // Методы const и трогают только token_, поэтому GetUpdates (поток команд) и Send (оба потока)
    // можно вызывать одновременно. Токен входит в URL запроса — URL и ошибки cpr не логируем.
    class Bot final : public IMessageSender {
    public:
        // token — токен бота, выданный @BotFather.
        explicit Bot(std::string token);

        // Long polling getUpdates: ждёт новых сообщений до нескольких секунд и сдвигает offset.
        // При ошибке сети или непонятном ответе возвращает пустой список (после паузы),
        // а не роняет процесс.
        [[nodiscard]] std::vector<IncomingMessage> GetUpdates(std::int64_t &offset) const;

        // sendMessage с таймаутом 10 с.
        SendResult Send(std::int64_t chat_id, const std::string &text) override;
        [[nodiscard]] SendResult SendText(std::int64_t chat_id, const std::string &text) const;

    private:
        std::string ApiUrl(const std::string& method) const;

        std::string token_;
    };

}  // namespace telegram
