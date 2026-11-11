#pragma once

#include "site_status_event.h"

namespace kafka {

    // Отправка события смены статуса. Шов между потоком записи чекера и Kafka: тесты подменяют
    // его фейком и проверяют порядок «транзакция → отправка → отметка» без брокера.
    class IEventPublisher {
    public:
        virtual ~IEventPublisher() = default;

        // Синхронно: true — брокер подтвердил приём, false — не подтвердил за таймаут или отказал.
        // При false событие может всё же дойти позже — тогда после досылки будет дубль, а не
        // потеря.
        [[nodiscard]] virtual bool Publish(const SiteStatusEvent &event) = 0;
    };

} // namespace kafka
