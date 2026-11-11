#pragma once

#include "../storage/ibot_store.h"

#include <pqxx/pqxx>

#include <string>

namespace PG {

    // Хранилище бота на Postgres: подписки и проверка адресов по списку сайтов.
    // Соединение своё у каждого объекта — каждому потоку бота свой BotStore.
    class BotStore final : public storage::IBotStore {
    public:
        explicit BotStore(const std::string &database_url);

        std::set<std::string> KnownSites(const std::vector<std::string> &urls) override;
        void ReplaceSubscriptions(std::int64_t chat_id,
                                  const std::vector<std::string> &urls) override;
        std::vector<std::int64_t> Subscribers(const std::string &url) override;

    private:
        pqxx::connection conn_;
    };

} // namespace PG
