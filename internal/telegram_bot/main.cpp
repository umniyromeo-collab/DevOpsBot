// TelegramBot — процесс бота: принимает подписки (/subscribe) и доставляет подписчикам события
// о смене статуса сайтов из Kafka. Контракты — в CLAUDE.md, раздел «Bot contracts».

#include "../db/pg_bot_store.h"
#include "../kafka/consumer.h"
#include "bot.h"
#include "commands.h"
#include "notifier.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>
#include <thread>

namespace {

    // Флаг остановки. Взводят обработчик SIGINT/SIGTERM и потоки бота при ошибке.
    std::atomic<bool> g_stop{false};
    static_assert(std::atomic<bool>::is_always_lock_free);

    void OnSignal(int) {
        g_stop.store(true);
    }

    std::string EnvOr(const char *name, const std::string &fallback) {
        const char *value = std::getenv(name);
        return value != nullptr && *value != '\0' ? value : fallback;
    }

    // Пауза между повторами отправки (до 30 с), которую прерывает флаг остановки.
    void InterruptibleSleep(std::chrono::milliseconds pause) {
        const auto deadline = std::chrono::steady_clock::now() + pause;
        while (!g_stop.load()) {
            const auto now = std::chrono::steady_clock::now();
            if (now >= deadline) {
                return;
            }
            std::this_thread::sleep_for(std::min<std::chrono::steady_clock::duration>(
                std::chrono::milliseconds{200}, deadline - now));
        }
    }

    // Поток команд: getUpdates → /subscribe → подписки в БД → ответ пользователю.
    void RunCommands(const telegram::Bot &bot, storage::IBotStore &store) {
        telegram::CommandHandler handler(store);
        std::int64_t offset = 0;
        while (!g_stop.load()) {
            for (const auto &message : bot.GetUpdates(offset)) {
                const std::string reply = handler.Handle(message.chat_id, message.text);
                if (bot.SendText(message.chat_id, reply).outcome != telegram::SendOutcome::Sent) {
                    std::cerr << "Чат " << message.chat_id << ": не удалось ответить" << std::endl;
                }
            }
        }
    }

    // На любом выходе из main, в том числе по исключению, останавливает поток команд и
    // дожидается его: иначе ~std::thread вызвал бы terminate.
    class StopAndJoin {
    public:
        explicit StopAndJoin(std::thread &thread) : thread_(thread) {}
        ~StopAndJoin() {
            Finish();
        }

        StopAndJoin(const StopAndJoin &) = delete;
        StopAndJoin &operator=(const StopAndJoin &) = delete;

        void Finish() {
            g_stop = true;
            if (thread_.joinable()) {
                thread_.join();
            }
        }

    private:
        std::thread &thread_;
    };

} // namespace

int main() {
    std::signal(SIGINT, OnSignal);
    std::signal(SIGTERM, OnSignal);

    // Токен не хардкодим — берём из окружения, чтобы не утёк в git.
    const char* token = std::getenv("TELEGRAM_BOT_TOKEN");
    if (token == nullptr || *token == '\0') {
        std::cerr << "Не задан TELEGRAM_BOT_TOKEN. "
                     "Получите токен у @BotFather и экспортируйте его:\n"
                     "  export TELEGRAM_BOT_TOKEN=\"...\"" << std::endl;
        return 1;
    }
    const char *database_url = std::getenv("DATABASE_URL");
    if (database_url == nullptr || *database_url == '\0') {
        std::cerr << "Не задан DATABASE_URL (postgresql://user:pass@localhost:5432/db)"
                  << std::endl;
        return 1;
    }
    const std::string brokers = EnvOr("KAFKA_BROKERS", "localhost:9092");

    try {
        telegram::Bot bot(token);
        // У каждого потока своё соединение: pqxx::connection не потокобезопасен.
        PG::BotStore commands_store(database_url);
        PG::BotStore delivery_store(database_url);
        kafka::Consumer consumer(brokers, "tgbot");

        std::atomic<bool> failed{false};
        std::thread commands_thread([&] {
            try {
                RunCommands(bot, commands_store);
            } catch (const std::exception &error) {
                std::cerr << "Поток команд остановлен: " << error.what() << std::endl;
                failed = true;
            } catch (...) {
                std::cerr << "Поток команд остановлен" << std::endl;
                failed = true;
            }
            g_stop = true;
        });
        StopAndJoin join_commands(commands_thread);

        std::cout << "TelegramBot: жду подписки и события" << std::endl;

        // Поток доставки — главный поток.
        telegram::Notifier notifier(consumer, delivery_store, bot, g_stop, InterruptibleSleep);
        try {
            notifier.Run();
        } catch (const std::exception &error) {
            std::cerr << "Поток доставки остановлен: " << error.what() << std::endl;
            failed = true;
        }

        join_commands.Finish();
        return failed ? 1 : 0;
    } catch (const std::exception &error) {
        std::cerr << "TelegramBot: " << error.what() << std::endl;
        return 1;
    }
}
