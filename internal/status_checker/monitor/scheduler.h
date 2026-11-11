#pragma once

#include "checker.h"
#include "checker_result.h"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace monitor {

    // Планировщик проверок. Живёт в одном потоке (главном): расписание принадлежит только ему,
    // поэтому без мьютекса. С воркерами пула общий лишь флаг in_flight сайта.
    //
    // in_flight держится от постановки задачи до передачи результата на запись. Пока он поднят,
    // новая проверка сайта не ставится, поэтому задач в пуле не больше числа сайтов, а заполненная
    // очередь записи (push блокируется → флаг не снимается) притормаживает и сам планировщик.
    class Scheduler {
    public:
        using Clock = std::chrono::steady_clock;
        using Task = std::function<void()>;
        // Ставит задачу в пул — ThreadPool::Execute.
        using Executor = std::function<void(Task)>;
        // Передаёт результат на запись — push в очередь; false — очередь закрыта.
        using Sink = std::function<bool(CheckerResult)>;
        // Нужна ли ещё проверка. При остановке задачи, ждущие в очереди пула, не ходят в сеть зря.
        using ShouldRun = std::function<bool()>;

        struct Site {
            std::shared_ptr<IChecker> checker;
            std::chrono::seconds base_interval;
        };

        Scheduler(std::vector<Site> sites, Executor executor, Sink sink, ShouldRun should_run,
                  Clock::time_point start);

        // Ставит в пул проверки сайтов, которым подошёл срок и у которых нет незавершённой
        // проверки. Сайт с незавершённой проверкой пропускает этот срок. Возвращает ближайший срок.
        Clock::time_point Tick(Clock::time_point now);

        // Пересчитывает интервалы по числу недавних неудач (ключ — URL, сайтов без неудач нет).
        void UpdateIntervals(const std::map<std::string, int> &recent_failures);

        [[nodiscard]] std::chrono::seconds IntervalOf(const std::string &url) const;
        [[nodiscard]] std::size_t SiteCount() const {
            return entries_.size();
        }

    private:
        struct Entry {
            std::string url;
            std::shared_ptr<IChecker> checker;
            std::chrono::seconds base_interval;
            std::chrono::seconds interval;
            Clock::time_point last_run;
            Clock::time_point next_run;
            // shared_ptr: задачу захватывают по значению (правило 6), флаг живёт, пока жива задача.
            std::shared_ptr<std::atomic<bool>> in_flight;
        };

        void Submit(const Entry &entry);

        std::vector<Entry> entries_;
        Executor executor_;
        Sink sink_;
        ShouldRun should_run_;
    };

} // namespace monitor
