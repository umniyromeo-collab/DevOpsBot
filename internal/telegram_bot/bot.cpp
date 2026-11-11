#include "bot.h"
#include "cpr/cpr.h"

#include <nlohmann/json.hpp>

#include <chrono>
#include <iostream>
#include <thread>
#include <utility>

namespace telegram {

    namespace {

        // Long polling держит соединение до kPollSeconds; таймаут cpr — с запасом сверху.
        // Короткий опрос, чтобы при остановке поток команд выходил за секунды, а не за минуту.
        constexpr int kPollSeconds = 5;
        constexpr int kPollTimeoutMs = 15000;
        constexpr int kSendTimeoutMs = 10000;

    } // namespace

    Bot::Bot(std::string token) : token_(std::move(token)) {}

    std::string Bot::ApiUrl(const std::string &method) const {
        return "https://api.telegram.org/bot" + token_ + "/" + method;
    }

    SendResult Bot::Send(std::int64_t chat_id, const std::string &text) {
        return SendText(chat_id, text);
    }

    SendResult Bot::SendText(std::int64_t chat_id, const std::string &text) const {
        const cpr::Response response =
            cpr::Post(cpr::Url{ApiUrl("sendMessage")},
                      cpr::Payload{{"chat_id", std::to_string(chat_id)}, {"text", text}},
                      cpr::Timeout{kSendTimeoutMs});
        return ClassifySendResponse(static_cast<int>(response.status_code), response.text);
    }

    std::vector<IncomingMessage> Bot::GetUpdates(std::int64_t &offset) const {
        const cpr::Response response =
            cpr::Get(cpr::Url{ApiUrl("getUpdates")},
                     cpr::Parameters{{"offset", std::to_string(offset)},
                                     {"timeout", std::to_string(kPollSeconds)}},
                     cpr::Timeout{kPollTimeoutMs});

        // Без исключений: при сетевой ошибке ответ пустой, и раньше процесс падал на parse.
        const auto parsed =
            nlohmann::json::parse(response.text, nullptr, /*allow_exceptions=*/false);
        if (parsed.is_discarded() || !parsed.is_object() || !parsed.value("ok", false) ||
            !parsed.contains("result") || !parsed["result"].is_array()) {
            // URL с токеном не печатаем — только код ответа.
            std::cerr << "Telegram getUpdates: нет ответа или ошибка (HTTP " << response.status_code
                      << "), повторю" << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds{1});
            return {};
        }

        std::vector<IncomingMessage> messages;
        for (const auto &update : parsed["result"]) {
            if (!update.is_object() || !update.contains("update_id") ||
                !update["update_id"].is_number_integer()) {
                continue;
            }
            // Offset сдвигаем и для апдейтов без текста: иначе Telegram отдавал бы их вечно.
            offset = update["update_id"].get<std::int64_t>() + 1;

            if (!update.contains("message") || !update["message"].is_object()) {
                continue;
            }
            const auto &message = update["message"];
            if (!message.contains("text") || !message["text"].is_string() ||
                !message.contains("chat") || !message["chat"].contains("id") ||
                !message["chat"]["id"].is_number_integer()) {
                continue;
            }
            messages.push_back(
                {message["chat"]["id"].get<std::int64_t>(), message["text"].get<std::string>()});
        }
        return messages;
    }
} // namespace telegram
