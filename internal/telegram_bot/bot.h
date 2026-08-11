#pragma once

#include <string>
#include <cstdint>

namespace telegram {

    // Простейший клиент Telegram Bot API поверх cpr (libcurl).
    //
    // Работает через long polling: getUpdates с таймаутом держит соединение
    // открытым, пока не придёт новое сообщение либо не истечёт таймаут.
    // На каждое входящее сообщение печатает его текст в консоль и отвечает
    // текущим временем сервера.
    class Bot final {
    public:
        // token — токен бота, выданный @BotFather.
        explicit Bot(std::string token);

        // Запускает бесконечный цикл опроса. Блокирует поток вызова.
        void Run();

    private:
        // Один шаг опроса: забирает пачку апдейтов и обрабатывает их.
        // Возвращает offset для следующего запроса (id последнего апдейта + 1).
        int64_t Poll(int64_t offset);

        // Отправляет текстовое сообщение в чат.
        void SendText(int64_t chat_id, const std::string& text) const;

        std::string ApiUrl(const std::string& method) const;

        std::string token_;
    };

}  // namespace telegram
