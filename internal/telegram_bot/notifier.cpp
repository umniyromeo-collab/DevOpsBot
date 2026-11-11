#include "notifier.h"

#include "commands.h"

#include <algorithm>
#include <iostream>
#include <string>
#include <utility>

namespace telegram {

    namespace {

        constexpr std::chrono::seconds kFirstBackoff{1};
        constexpr std::chrono::seconds kMaxBackoff{30};

    } // namespace

    Notifier::Notifier(kafka::IEventSource &source, storage::IBotStore &store,
                       IMessageSender &sender, const std::atomic<bool> &stop, Sleep sleep)
        : source_(source), store_(store), sender_(sender), stop_(stop), sleep_(std::move(sleep)) {}

    bool Notifier::Deliver(const kafka::SiteStatusEvent &event) {
        const std::string text = StatusMessage(event);
        for (const std::int64_t chat_id : store_.Subscribers(event.url)) {
            std::chrono::seconds backoff = kFirstBackoff;
            while (true) {
                const SendResult result = sender_.Send(chat_id, text);
                if (result.outcome == SendOutcome::Sent) {
                    break;
                }
                if (result.outcome == SendOutcome::Skip) {
                    std::cerr << "Чат " << chat_id << ": Telegram отклонил сообщение по "
                              << event.url << ", получатель пропущен" << std::endl;
                    break;
                }
                if (stop_.load()) {
                    return false;
                }
                sleep_(result.retry_after.count() > 0 ? result.retry_after : backoff);
                backoff = std::min(backoff * 2, kMaxBackoff);
                if (stop_.load()) {
                    return false;
                }
            }
        }
        return true;
    }

    void Notifier::Run() {
        while (!stop_.load()) {
            const auto event = source_.Next(std::chrono::seconds{1});
            if (!event) {
                continue;
            }
            if (!Deliver(*event)) {
                return; // без коммита: после перезапуска событие придёт снова
            }
            source_.Commit();
        }
    }

} // namespace telegram
