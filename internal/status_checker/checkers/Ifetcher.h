#pragma once

#include <string>

namespace net {

    // "Шов" (seam) между чекером и сетью.
    // Делает запрос по URL и возвращает HTTP-код ответа.
    // 0 — ответа не было вовсе (DNS не разрешился, таймаут, отказ соединения).
    //
    // В проде здесь будет реализация на libcurl, а в юнит-тестах — фейк,
    // который возвращает заранее заданный код и в сеть не ходит. Именно
    // благодаря этому интерфейсу логику Check() можно проверить без сети.
    class IFetcher {
    public:
        virtual ~IFetcher() = default;
        [[nodiscard]] virtual int FetchStatusCode(const std::string& url) const = 0;
    };

}  // namespace net
