#include "scheduler.h"

#include "priority.h"

#include <algorithm>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <utility>

namespace monitor {

    Scheduler::Scheduler(std::vector<Site> sites, Executor executor, Sink sink,
                         ShouldRun should_run, Clock::time_point start)
        : executor_(std::move(executor)), sink_(std::move(sink)),
          should_run_(std::move(should_run)) {
        entries_.reserve(sites.size());
        for (auto &site : sites) {
            Entry entry;
            entry.url = site.checker->URL();
            entry.checker = std::move(site.checker);
            entry.base_interval = site.base_interval;
            entry.interval = site.base_interval;
            // Первая проверка — сразу: «прошлый запуск» был ровно интервал назад.
            entry.last_run = start - entry.interval;
            entry.next_run = start;
            entry.in_flight = std::make_shared<std::atomic<bool>>(false);
            entries_.push_back(std::move(entry));
        }
    }

    Scheduler::Clock::time_point Scheduler::Tick(Clock::time_point now) {
        Clock::time_point nearest = Clock::time_point::max();
        for (auto &entry : entries_) {
            if (entry.next_run <= now) {
                // acquire — пара к release в задаче: видим всё, что задача сделала до снятия флага.
                if (!entry.in_flight->load(std::memory_order_acquire)) {
                    entry.in_flight->store(true, std::memory_order_relaxed);
                    entry.last_run = now;
                    Submit(entry);
                }
                entry.next_run = now + entry.interval;
            }
            nearest = std::min(nearest, entry.next_run);
        }
        return nearest;
    }

    void Scheduler::UpdateIntervals(const std::map<std::string, int> &recent_failures) {
        for (auto &entry : entries_) {
            const auto it = recent_failures.find(entry.url);
            const int failures = it != recent_failures.end() ? it->second : 0;
            const std::chrono::seconds interval = EffectiveInterval(entry.base_interval, failures);
            if (interval == entry.interval) {
                continue;
            }
            std::cout << entry.url << ": интервал " << entry.interval.count() << " с -> "
                      << interval.count() << " с" << std::endl;
            entry.interval = interval;
            // Отсчёт от последнего запуска: сайт со свежим сбоем проверится раньше, не дожидаясь
            // срока, назначенного по старому интервалу.
            entry.next_run = entry.last_run + interval;
        }
    }

    std::chrono::seconds Scheduler::IntervalOf(const std::string &url) const {
        for (const auto &entry : entries_) {
            if (entry.url == url) {
                return entry.interval;
            }
        }
        throw std::out_of_range("Scheduler: нет сайта " + url);
    }

    void Scheduler::Submit(const Entry &entry) {
        // Всё по значению: задача не должна зависеть от времени жизни планировщика (правило 6).
        executor_([checker = entry.checker, in_flight = entry.in_flight, sink = sink_,
                   should_run = should_run_] {
            // Исключение из задачи — std::terminate всему процессу (у воркера нет try/catch),
            // поэтому ловим всё здесь.
            try {
                if (should_run()) {
                    (void)sink(checker->Check());
                }
            } catch (const std::exception &error) {
                std::cerr << checker->URL() << ": проверка упала: " << error.what() << std::endl;
            } catch (...) {
                std::cerr << checker->URL() << ": проверка упала" << std::endl;
            }
            // Снимаем флаг после push: пока результат не передан на запись, новая проверка
            // сайта не нужна.
            in_flight->store(false, std::memory_order_release);
        });
    }

} // namespace monitor
