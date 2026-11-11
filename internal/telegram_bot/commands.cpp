#include "commands.h"

#include <algorithm>
#include <chrono>
#include <format>

namespace telegram {

    namespace {

        constexpr std::string_view kCommand = "/subscribe";

        bool IsSeparator(char c) {
            return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == ',';
        }

        std::vector<std::string_view> Split(std::string_view text) {
            std::vector<std::string_view> tokens;
            std::size_t i = 0;
            while (i < text.size()) {
                while (i < text.size() && IsSeparator(text[i])) {
                    ++i;
                }
                const std::size_t start = i;
                while (i < text.size() && !IsSeparator(text[i])) {
                    ++i;
                }
                if (i > start) {
                    tokens.push_back(text.substr(start, i - start));
                }
            }
            return tokens;
        }

        void AppendList(std::string &out, const std::vector<std::string> &urls) {
            for (const auto &url : urls) {
                out += "\n• " + url;
            }
        }

    } // namespace

    std::optional<SubscribeCommand> ParseSubscribe(std::string_view text) {
        const auto tokens = Split(text);
        if (tokens.empty()) {
            return std::nullopt;
        }
        // В группах Telegram добавляет к команде имя бота: /subscribe@MyBot.
        const std::string_view head = tokens.front();
        if (head != kCommand && !(head.starts_with(kCommand) && head.size() > kCommand.size() &&
                                  head[kCommand.size()] == '@')) {
            return std::nullopt;
        }

        SubscribeCommand command;
        for (std::size_t i = 1; i < tokens.size(); ++i) {
            const std::string url{tokens[i]};
            if (std::find(command.urls.begin(), command.urls.end(), url) == command.urls.end()) {
                command.urls.push_back(url);
            }
        }
        return command;
    }

    SubscribePlan PlanSubscribe(const SubscribeCommand &command,
                                const std::set<std::string> &known) {
        SubscribePlan plan;
        for (const auto &url : command.urls) {
            (known.contains(url) ? plan.subscriptions : plan.rejected).push_back(url);
        }
        // Пустая команда — снять все подписки. Иначе меняем, только если есть что оставить:
        // команда из одних опечаток не должна молча стирать прежние подписки.
        plan.apply = command.urls.empty() || !plan.subscriptions.empty();
        return plan;
    }

    std::string SubscribeReply(const SubscribePlan &plan) {
        std::string reply;
        if (plan.apply && plan.subscriptions.empty()) {
            reply = "Подписки сняты, уведомлений больше не будет.";
        } else if (plan.apply) {
            reply = "Подписки:";
            AppendList(reply, plan.subscriptions);
        } else {
            reply = "Подписки не изменены: ни одного адреса из списка сайтов.";
        }
        if (!plan.rejected.empty()) {
            reply += "\n\nНет в списке сайтов, пропущены:";
            AppendList(reply, plan.rejected);
        }
        return reply;
    }

    std::string HelpText() {
        return "Отправьте /subscribe и адреса сайтов через пробел, например:\n"
               "/subscribe https://example.com https://github.com\n\n"
               "Список заменяет прежние подписки. /subscribe без адресов снимает все.";
    }

    std::string StatusMessage(const kafka::SiteStatusEvent &event) {
        const std::chrono::sys_seconds when{std::chrono::seconds{event.changed_at}};
        const std::string time = std::format("{:%Y-%m-%d %H:%M:%S} UTC", when);
        if (event.is_up) {
            return std::format("🟢 Сайт снова работает: {}\n{}", event.url, time);
        }
        const std::string reason = event.http_code == 0 ? std::string{"нет ответа"}
                                                        : std::format("код {}", event.http_code);
        return std::format("🔴 Сайт упал: {} ({})\n{}", event.url, reason, time);
    }

    std::string CommandHandler::Handle(std::int64_t chat_id, std::string_view text) {
        const auto command = ParseSubscribe(text);
        if (!command) {
            return HelpText();
        }
        const SubscribePlan plan = PlanSubscribe(*command, store_.KnownSites(command->urls));
        if (plan.apply) {
            store_.ReplaceSubscriptions(chat_id, plan.subscriptions);
        }
        return SubscribeReply(plan);
    }

} // namespace telegram
