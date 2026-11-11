#pragma once

#include "../kafka/event_source.h"
#include "../storage/ibot_store.h"
#include "message_sender.h"

#include <atomic>
#include <chrono>
#include <functional>

namespace telegram {

    // Поток доставки бота: событие из Kafka → подписчики из БД → sendMessage каждому →
    // коммит offset. Коммит только после всех получателей: упал раньше — после перезапуска
    // событие придёт снова (возможен дубль, но не потеря).
    class Notifier {
    public:
        // Пауза между повторами; должна прерываться флагом остановки.
        using Sleep = std::function<void(std::chrono::milliseconds)>;

        Notifier(kafka::IEventSource &source, storage::IBotStore &store, IMessageSender &sender,
                 const std::atomic<bool> &stop, Sleep sleep);

        // Отправляет событие всем подписчикам. Временные ошибки повторяет, окончательные
        // пропускает. false — остановлено раньше, чем все получили: коммитить нельзя.
        [[nodiscard]] bool Deliver(const kafka::SiteStatusEvent &event);

        // Тело потока доставки: события по одному, в порядке топика, пока не взведён stop.
        void Run();

    private:
        kafka::IEventSource &source_;
        storage::IBotStore &store_;
        IMessageSender &sender_;
        const std::atomic<bool> &stop_;
        Sleep sleep_;
    };

} // namespace telegram
