#pragma once

#include <cstdint>
#include <set>
#include <string>
#include <vector>

namespace storage {

    // Хранилище процесса бота. В заголовке нет libpqxx, тесты подменяют его фейком.
    // Объект не потокобезопасен — у каждого потока свой.
    class IBotStore {
    public:
        virtual ~IBotStore() = default;

        // Какие из переданных адресов есть в фиксированном списке сайтов.
        [[nodiscard]] virtual std::set<std::string>
        KnownSites(const std::vector<std::string> &urls) = 0;

        // Заменяет все подписки пользователя одной транзакцией. Пустой список снимает все.
        virtual void ReplaceSubscriptions(std::int64_t chat_id,
                                          const std::vector<std::string> &urls) = 0;

        // Кто подписан на сайт.
        [[nodiscard]] virtual std::vector<std::int64_t> Subscribers(const std::string &url) = 0;
    };

} // namespace storage
