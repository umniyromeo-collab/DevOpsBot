#pragma once

#include "../kafka/site_status_event.h"
#include "../storage/ibot_store.h"

#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace telegram {

    // Команда /subscribe <url> [<url> ...]. Адреса — без повторов, в порядке ввода.
    struct SubscribeCommand {
        std::vector<std::string> urls;
    };

    // nullopt — сообщение не является командой /subscribe (в том числе /subscribe@ИмяБота).
    [[nodiscard]] std::optional<SubscribeCommand> ParseSubscribe(std::string_view text);

    // Что сделать с подписками после проверки адресов по списку сайтов.
    struct SubscribePlan {
        // Менять ли подписки. false — ни одного адреса из списка: прежние подписки остаются.
        bool apply = false;
        std::vector<std::string> subscriptions;
        std::vector<std::string> rejected;
    };

    // known — какие из адресов команды есть в списке сайтов. Пустая команда снимает все подписки.
    [[nodiscard]] SubscribePlan PlanSubscribe(const SubscribeCommand &command,
                                              const std::set<std::string> &known);

    [[nodiscard]] std::string SubscribeReply(const SubscribePlan &plan);
    [[nodiscard]] std::string HelpText();

    // Текст уведомления о смене статуса сайта.
    [[nodiscard]] std::string StatusMessage(const kafka::SiteStatusEvent &event);

    // Обработка входящего сообщения: подписки в БД и текст ответа. Работает в потоке команд
    // со своим хранилищем.
    class CommandHandler {
    public:
        explicit CommandHandler(storage::IBotStore &store) : store_(store) {}

        // Ошибки БД пробрасывает наружу.
        [[nodiscard]] std::string Handle(std::int64_t chat_id, std::string_view text);

    private:
        storage::IBotStore &store_;
    };

} // namespace telegram
