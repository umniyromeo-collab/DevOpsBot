#pragma once

// Фейковое хранилище бота для юнит-тестов: список сайтов и подписки в памяти.

#include "../storage/ibot_store.h"

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace telegram::testing {

    class FakeBotStore final : public storage::IBotStore {
    public:
        std::set<std::string> sites;
        std::map<std::int64_t, std::vector<std::string>> subscriptions;
        int replace_calls = 0;

        std::set<std::string> KnownSites(const std::vector<std::string> &urls) override {
            std::set<std::string> known;
            for (const auto &url : urls) {
                if (sites.contains(url)) {
                    known.insert(url);
                }
            }
            return known;
        }

        void ReplaceSubscriptions(std::int64_t chat_id,
                                  const std::vector<std::string> &urls) override {
            ++replace_calls;
            subscriptions[chat_id] = urls;
        }

        std::vector<std::int64_t> Subscribers(const std::string &url) override {
            std::vector<std::int64_t> chats;
            for (const auto &[chat_id, urls] : subscriptions) {
                if (std::find(urls.begin(), urls.end(), url) != urls.end()) {
                    chats.push_back(chat_id);
                }
            }
            return chats;
        }
    };

} // namespace telegram::testing
