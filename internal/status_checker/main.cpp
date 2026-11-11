// DevOpsBot — процесс чекера: проверяет сайты из таблицы sites, пишет результаты в PostgreSQL
// и отправляет в Kafka события о смене статуса. Схема потоков — в .claude/CLAUDE.md
// и openspec/changes/site-down-notifications/design.md.

#include <algorithm>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "../db/pg_checker_store.h"
#include "../kafka/producer.h"
#include "../lib/thread_pools/thread_pool.h"
#include "../lib/thread_queue/thread_queue.h"
#include "checkers/builder/builder.h"
#include "checkers/http/fetcher.h"
#include "monitor/priority_updater.h"
#include "monitor/result_writer.h"
#include "monitor/scheduler.h"

namespace {

    // Флаг остановки. Взводят обработчик SIGINT/SIGTERM и поток записи при ошибке.
    // Lock-free atomic можно трогать из обработчика сигнала.
    std::atomic<bool> g_stop{false};
    static_assert(std::atomic<bool>::is_always_lock_free);

    void OnSignal(int) {
        g_stop.store(true);
    }

    std::string EnvOr(const char *name, const std::string &fallback) {
        const char *value = std::getenv(name);
        return value != nullptr && *value != '\0' ? value : fallback;
    }

    std::size_t ThreadsFromEnv() {
        const std::string raw = EnvOr("CHECKER_THREADS", "32");
        try {
            const long threads = std::stol(raw);
            if (threads > 0 && threads <= 1024) {
                return static_cast<std::size_t>(threads);
            }
        } catch (const std::exception &) {
        }
        std::cerr << "CHECKER_THREADS=" << raw << " — нужно число от 1 до 1024, беру 32"
                  << std::endl;
        return 32;
    }

    // Закрывает очередь и дожидается потока записи при любом выходе из main, в том числе
    // по исключению: иначе поток так и спал бы в wait_and_pop, а ~std::thread вызвал бы terminate.
    // Объявлен после потока записи, поэтому разрушается раньше него, но после пула.
    class WriterGuard {
    public:
        WriterGuard(BoundedThreadSafeQueue<CheckerResult> &queue, std::thread &thread)
            : queue_(queue), thread_(thread) {}
        ~WriterGuard() {
            Finish();
        }

        WriterGuard(const WriterGuard &) = delete;
        WriterGuard &operator=(const WriterGuard &) = delete;

        // Поток записи дочитывает остаток очереди и выходит. Повторный вызов безопасен.
        void Finish() {
            queue_.close();
            if (thread_.joinable()) {
                thread_.join();
            }
        }

    private:
        BoundedThreadSafeQueue<CheckerResult> &queue_;
        std::thread &thread_;
    };

} // namespace

int main() {
    std::signal(SIGINT, OnSignal);
    std::signal(SIGTERM, OnSignal);

    const char *database_url = std::getenv("DATABASE_URL");
    if (database_url == nullptr || *database_url == '\0') {
        std::cerr << "Не задан DATABASE_URL (postgresql://user:pass@localhost:5432/db)"
                  << std::endl;
        return 1;
    }
    const std::string brokers = EnvOr("KAFKA_BROKERS", "localhost:9092");
    const std::size_t threads = ThreadsFromEnv();

    try {
        // У каждого потока своё соединение: pqxx::connection не потокобезопасен.
        PG::CheckerStore writer_store(database_url);    // поток записи
        PG::CheckerStore scheduler_store(database_url); // главный поток: сайты и история сбоев
        kafka::Producer producer(brokers, std::chrono::seconds{5});

        // Чекеры хранят ссылку на фетчер: он объявлен раньше пула и переживёт все задачи.
        HTTPChecker::Fetcher fetcher;
        std::vector<monitor::Scheduler::Site> sites;
        for (const auto &site : scheduler_store.LoadSites()) {
            const auto config = Builder::ConfigFromSite(site.url, site.expected_http_code);
            if (!config) {
                std::cerr << site.url << ": схема не поддерживается (нужен http:// или https://), "
                          << "сайт пропущен" << std::endl;
                continue;
            }
            sites.push_back({std::shared_ptr<IChecker>(Builder::CreateChecker(*config, fetcher)),
                             site.base_interval});
        }
        if (sites.empty()) {
            std::cerr << "В таблице sites нет сайтов для проверки: примените "
                      << "internal/db/sites_seed.sql или добавьте свои" << std::endl;
            return 1;
        }

        // Очередь объявлена раньше пула и потока записи: разрушится после них, когда все,
        // кто её трогает, уже завершены. Ёмкость — число сайтов: из-за in_flight больше
        // результатов одновременно не бывает.
        BoundedThreadSafeQueue<CheckerResult> queue(sites.size());

        monitor::ResultWriter writer(writer_store, producer);
        // Досылка — до первой проверки (см. ResultWriter::Recover). Дальше writer_store и producer
        // использует только поток записи.
        writer.Recover();

        std::atomic<bool> writer_failed{false};
        std::thread writer_thread([&] {
            try {
                writer.Run(queue);
            } catch (const std::exception &error) {
                std::cerr << "Поток записи остановлен: " << error.what() << std::endl;
                writer_failed = true;
            } catch (...) {
                std::cerr << "Поток записи остановлен" << std::endl;
                writer_failed = true;
            }
            // Без записи процессу работать незачем: освобождаем воркеров, застрявших в push,
            // и останавливаем планировщик — перезапуск дошлёт всё несохранённое.
            queue.close();
            g_stop = true;
        });
        WriterGuard writer_guard(queue, writer_thread);

        std::cout << "DevOpsBot: " << sites.size() << " сайтов, " << threads << " потоков"
                  << std::endl;
        {
            ThreadPool pool(threads);
            monitor::Scheduler scheduler(
                std::move(sites),
                [&pool](monitor::Scheduler::Task task) { pool.Execute(std::move(task)); },
                [&queue](CheckerResult result) { return queue.push(std::move(result)); },
                [] { return !g_stop.load(); }, monitor::Scheduler::Clock::now());
            monitor::PriorityUpdater updater(scheduler_store, monitor::Scheduler::Clock::now());

            try {
                while (!g_stop.load()) {
                    const auto now = monitor::Scheduler::Clock::now();
                    updater.MaybeUpdate(now, scheduler);
                    const auto next = scheduler.Tick(now);
                    // Не дольше секунды: так флаг остановки замечается быстро.
                    std::this_thread::sleep_until(std::min(next, now + std::chrono::seconds{1}));
                }
            } catch (...) {
                // Взводим флаг до разрушения пула, чтобы задачи из его очереди не ходили в сеть.
                g_stop = true;
                throw;
            }
            std::cout << "Остановка: дожидаюсь начатых проверок" << std::endl;
        } // ~ThreadPool: задачи из очереди пула выходят сразу, начатые проверки доделываются

        writer_guard.Finish(); // поток записи дочитывает остаток очереди
        return writer_failed ? 1 : 0;
    } catch (const std::exception &error) {
        std::cerr << "DevOpsBot: " << error.what() << std::endl;
        return 1;
    }
}
