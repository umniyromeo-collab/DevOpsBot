#pragma once

#include "site_status_event.h"

#include <chrono>
#include <optional>

namespace kafka {

    // Чтение событий смены статуса. Шов между ботом и Kafka для тестов.
    class IEventSource {
    public:
        virtual ~IEventSource() = default;

        // Следующее событие или nullopt, если за timeout ничего не пришло.
        [[nodiscard]] virtual std::optional<SiteStatusEvent>
        Next(std::chrono::milliseconds timeout) = 0;

        // Подтверждает обработку последнего события из Next: после перезапуска оно не придёт снова.
        // Не вызвал — событие придёт ещё раз, так бот не теряет сообщения при падении.
        virtual void Commit() = 0;
    };

} // namespace kafka
