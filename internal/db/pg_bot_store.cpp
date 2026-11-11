#include "pg_bot_store.h"

namespace PG {

    BotStore::BotStore(const std::string &database_url) : conn_(database_url) {}

    std::set<std::string> BotStore::KnownSites(const std::vector<std::string> &urls) {
        // Адресов в команде единицы, поэтому по запросу на адрес — проще, чем передавать массив.
        pqxx::read_transaction tx{conn_};
        std::set<std::string> known;
        for (const auto &url : urls) {
            if (!tx.exec("SELECT 1 FROM sites WHERE url = $1", pqxx::params{url}).empty()) {
                known.insert(url);
            }
        }
        return known;
    }

    void BotStore::ReplaceSubscriptions(std::int64_t chat_id,
                                        const std::vector<std::string> &urls) {
        // Одна транзакция: при падении посередине старые подписки остаются целиком.
        pqxx::work tx{conn_};
        tx.exec("DELETE FROM subscriptions WHERE chat_id = $1", pqxx::params{chat_id});
        for (const auto &url : urls) {
            tx.exec("INSERT INTO subscriptions (chat_id, url) VALUES ($1, $2) "
                    "ON CONFLICT DO NOTHING",
                    pqxx::params{chat_id, url});
        }
        tx.commit();
    }

    std::vector<std::int64_t> BotStore::Subscribers(const std::string &url) {
        pqxx::read_transaction tx{conn_};
        std::vector<std::int64_t> chats;
        for (const auto &row : tx.exec("SELECT chat_id FROM subscriptions WHERE url = $1 "
                                       "ORDER BY chat_id",
                                       pqxx::params{url})) {
            chats.push_back(row[0].as<std::int64_t>());
        }
        return chats;
    }

} // namespace PG
