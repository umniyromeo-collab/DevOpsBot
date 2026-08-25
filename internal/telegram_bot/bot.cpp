#include "bot.h"
#include "cpr/cpr.h"

#include <nlohmann/json.hpp>
#include <iostream>
#include <ctime>

namespace telegram {

    Bot::Bot(std::string token) : token_(token){}

    std::string Bot::ApiUrl(const std::string &method) const {
        return "https://api.telegram.org/bot" + token_ + "/" + method;
    }

    void Bot::SendText(int64_t chat_id, const std::string& text) const {

        cpr::Post(
            cpr::Url{ApiUrl("sendMessage")},
            cpr::Payload{
                {"chat_id", std::to_string(chat_id)},
                {"text", text}
            }
        );
    }

    int64_t Bot::Poll(int64_t offset) {

        auto ans = cpr::Get(

            cpr::Url{ApiUrl("getUpdates")},
            cpr::Parameters{
                {"offset", std::to_string(offset)},
                {"timeout", "15"}
            },
            cpr::Timeout{30000}
        );

        auto parsed_ans = nlohmann::json::parse(ans.text);

        auto new_offset = offset;

        for (const auto& mes : parsed_ans["result"]) {

            new_offset = mes.at("update_id").get<int64_t>() + 1;

            const auto chat_id = mes["message"]["chat"]["id"].get<int64_t>();

            std::cout << mes["message"]["text"].get<std::string>() << "\n";

            SendText(chat_id, std::to_string(std::time(nullptr)));
        }

        return new_offset;
    }

    void Bot::Run() {

        int64_t offset = 0;

        while (true) {
            offset = Poll(offset);
        }
    }
}