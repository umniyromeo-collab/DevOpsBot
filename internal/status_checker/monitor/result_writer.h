#pragma once

#include "../../kafka/event_publisher.h"
#include "../../lib/thread_queue/thread_queue.h"
#include "../../storage/ichecker_store.h"
#include "checker_result.h"
#include "site_status.h"

#include <cstdint>
#include <functional>
#include <map>
#include <string>

namespace monitor {

    // Поток записи чекера: единственный, кто пишет site_state. Порядок на каждый результат:
    //
    //   1. транзакция: строка check_results + новый статус сайта;
    //   2. если is_up != notified_is_up — событие в Kafka с ожиданием подтверждения;
    //   3. отметка notified_is_up = is_up.
    //
    // Падение после 1 оставляет расхождение, и событие досылается (Recover или следующий результат
    // сайта). Падение между 2 и 3 даёт дубль, но не потерю. Наоборот (сначала Kafka) нельзя:
    // упади процесс после отправки «упал», БД не узнала бы о ней, и «восстановился» не пришло бы.
    class ResultWriter {
    public:
        // Текущее unix-время в секундах; тесты подставляют своё.
        using UnixClock = std::function<std::int64_t()>;

        ResultWriter(storage::ICheckerStore &store, kafka::IEventPublisher &publisher,
                     UnixClock now = DefaultClock);

        // Загружает статусы и досылает события, сохранённые, но не подтверждённые Kafka.
        // Вызывать до запуска проверок: иначе сайт успеет восстановиться, статусы совпадут,
        // и о восстановлении никто не узнает.
        void Recover();

        // Обрабатывает результат одной проверки. Ошибки БД пробрасывает наружу: память
        // при этом не меняется, т.к. обновляется только после коммита.
        void Process(const CheckerResult &result);

        // Тело потока записи: обрабатывает результаты, пока очередь не закрыта и не пуста.
        void Run(BoundedThreadSafeQueue<CheckerResult> &queue);

        [[nodiscard]] const std::map<std::string, SiteStatus> &Statuses() const {
            return statuses_;
        }

    private:
        static std::int64_t DefaultClock();
        void PublishIfPending(const std::string &url, SiteStatus &status);

        storage::ICheckerStore &store_;
        kafka::IEventPublisher &publisher_;
        UnixClock now_;
        // Копия site_state в памяти: читать БД на каждый результат не нужно, пишет только этот
        // поток.
        std::map<std::string, SiteStatus> statuses_;
    };

} // namespace monitor
