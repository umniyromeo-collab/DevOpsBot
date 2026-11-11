cmake_minimum_required(VERSION 3.15)
project(DevOpsBot)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(internal/status_checker/checkers)

include(FetchContent)

# ---- GoogleTest ----
FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.15.2
)
FetchContent_MakeAvailable(googletest)

# ---- cpr (C++ Requests, обёртка над libcurl) для настоящего фетчера ----
if (NOT WIN32)
    # На mac/Linux libcurl есть в системе — пусть cpr переиспользует его.
    set(CPR_USE_SYSTEM_CURL ON)
endif ()
# На Windows cpr скачает и соберёт libcurl сам.
FetchContent_Declare(
        cpr
        GIT_REPOSITORY https://github.com/libcpr/cpr.git
        GIT_TAG 1.11.3
)
FetchContent_MakeAvailable(cpr)

# ---- nlohmann/json для разбора ответов Telegram Bot API ----
FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

# ---- librdkafka: клиент Kafka, через неё чекер и бот обмениваются событиями ----
# Брокер локальный и без шифрования, поэтому SSL/SASL и сжатие не собираем: меньше
# системных зависимостей и в образе, и на машине разработчика.
set(RDKAFKA_BUILD_STATIC ON CACHE BOOL "" FORCE)
set(RDKAFKA_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(RDKAFKA_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(WITH_SSL OFF CACHE BOOL "" FORCE)
set(WITH_SASL OFF CACHE BOOL "" FORCE)
set(WITH_ZLIB OFF CACHE BOOL "" FORCE)
set(WITH_ZSTD OFF CACHE BOOL "" FORCE)
set(WITH_CURL OFF CACHE BOOL "" FORCE)
set(ENABLE_LZ4_EXT OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
        librdkafka
        GIT_REPOSITORY https://github.com/confluentinc/librdkafka.git
        GIT_TAG v2.16.0
        GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(librdkafka)

# ---- Событие смены статуса: контракт между чекером и ботом ----
# Отдельно от клиента Kafka, чтобы тесты собирались без librdkafka.
add_library(kafka_event
        internal/kafka/site_status_event.cpp
)
target_link_libraries(kafka_event PUBLIC nlohmann_json::nlohmann_json)

add_library(kafka_client
        internal/kafka/producer.cpp
        internal/kafka/consumer.cpp
)
target_link_libraries(kafka_client PUBLIC kafka_event PRIVATE rdkafka++)

# ---- Telegram-бот: подписки (/subscribe) и доставка событий из Kafka ----
add_executable(TelegramBot
        internal/telegram_bot/main.cpp
        internal/telegram_bot/bot.cpp
        internal/telegram_bot/commands.cpp
        internal/telegram_bot/notifier.cpp
        internal/telegram_bot/send_result.cpp
)
target_link_libraries(TelegramBot PRIVATE cpr::cpr nlohmann_json::nlohmann_json storage
        kafka_client)

add_executable(DevOpsBot
        internal/status_checker/main.cpp
        internal/status_checker/checkers/builder/builder.cpp
        internal/status_checker/checkers/http/checker.cpp
        internal/status_checker/checkers/http/fetcher.cpp
        internal/status_checker/checkers/https/checker.cpp
        internal/status_checker/monitor/status_tracker.cpp
        internal/status_checker/monitor/priority.cpp
        internal/status_checker/monitor/result_writer.cpp
        internal/status_checker/monitor/scheduler.cpp
        internal/status_checker/monitor/priority_updater.cpp
        internal/lib/thread_pools/thread_pool.h
        internal/lib/thread_pools/thread_pool.cpp
        internal/lib/thread_queue/thread_queue.h
)
target_link_libraries(DevOpsBot PRIVATE cpr::cpr kafka_client)

# ---- Юнит-тесты на внутренности HTTP-чекера ----
# fetcher.cpp сюда НЕ добавляем: тесты подменяют сеть своим MockFetcher.
enable_testing()
include(GoogleTest)

add_executable(checker_test
        internal/status_checker/checkers/http/checker_test.cpp
        internal/status_checker/checkers/http/checker.cpp
        internal/status_checker/checkers/https/checker_test.cpp
        internal/status_checker/checkers/https/checker.cpp
        internal/status_checker/checkers/builder/builder_test.cpp
        internal/status_checker/checkers/builder/builder.cpp
        internal/lib/thread_queue/thread_queue_test.cpp
        internal/status_checker/monitor/status_tracker_test.cpp
        internal/status_checker/monitor/status_tracker.cpp
        internal/status_checker/monitor/priority_test.cpp
        internal/status_checker/monitor/priority.cpp
        internal/status_checker/monitor/fake_checker_store_test.cpp
        internal/kafka/site_status_event_test.cpp
        internal/status_checker/monitor/result_writer_test.cpp
        internal/status_checker/monitor/result_writer.cpp
        internal/status_checker/monitor/scheduler_test.cpp
        internal/status_checker/monitor/scheduler.cpp
        internal/status_checker/monitor/priority_updater.cpp
)
target_link_libraries(checker_test PRIVATE gtest_main cpr::cpr kafka_event)
# TIMEOUT: сломанное пробуждение в многопоточном тесте даёт вечный join, а не падение.
gtest_discover_tests(checker_test PROPERTIES TIMEOUT 60)

# ---- Юнит-тесты бота: команды, классификация ответов Telegram, доставка ----
# bot.cpp сюда НЕ добавляем: он ходит в Telegram, тесты подменяют его фейком IMessageSender.
add_executable(bot_test
        internal/telegram_bot/commands_test.cpp
        internal/telegram_bot/commands.cpp
        internal/telegram_bot/send_result_test.cpp
        internal/telegram_bot/send_result.cpp
        internal/telegram_bot/notifier_test.cpp
        internal/telegram_bot/notifier.cpp
)
target_link_libraries(bot_test PRIVATE gtest_main kafka_event)
gtest_discover_tests(bot_test PROPERTIES TIMEOUT 60)


# ---- libpqxx: официальный C++-клиент PostgreSQL (обёртка над libpq) ----
# Пути к libpq спрашиваем у pg_config, а не хардкодим: на macOS Homebrew
# ставит postgresql keg-only, в Alpine это пакет libpq-dev — префиксы разные.
find_program(PG_CONFIG_EXECUTABLE pg_config)
if (PG_CONFIG_EXECUTABLE)
    execute_process(COMMAND ${PG_CONFIG_EXECUTABLE} --includedir
            OUTPUT_VARIABLE PG_INCLUDEDIR OUTPUT_STRIP_TRAILING_WHITESPACE)
    execute_process(COMMAND ${PG_CONFIG_EXECUTABLE} --libdir
            OUTPUT_VARIABLE PG_LIBDIR OUTPUT_STRIP_TRAILING_WHITESPACE)
    set(PostgreSQL_INCLUDE_DIR "${PG_INCLUDEDIR}" CACHE PATH "libpq headers" FORCE)
    set(PostgreSQL_TYPE_INCLUDE_DIR "${PG_INCLUDEDIR}" CACHE PATH "libpq server headers" FORCE)
    find_library(PostgreSQL_LIBRARY NAMES pq HINTS "${PG_LIBDIR}")
endif ()

# Не собирать тесты самой libpqxx — нам нужна только библиотека.
set(SKIP_BUILD_TEST ON)
FetchContent_Declare(
        libpqxx
        GIT_REPOSITORY https://github.com/jtv/libpqxx.git
        GIT_TAG 7.10.7
)
FetchContent_MakeAvailable(libpqxx)

# ---- Хранилище результатов проверок (единственное место, где есть SQL) ----
# Интерфейс IStorage — в internal/storage, реализация на Postgres — в internal/db.
# PUBLIC: кто линкуется со storage, получает пути к storage.h / pg_storage.h и libpqxx.
add_library(storage
        internal/db/pg_storage.cpp
        internal/db/pg_checker_store.cpp
        internal/db/pg_bot_store.cpp
)
target_include_directories(storage PUBLIC internal/storage internal/db)
target_link_libraries(storage PUBLIC libpqxx::pqxx)

target_link_libraries(DevOpsBot PRIVATE storage)

# ---- Демонстрация записи событий бота в PostgreSQL ----
add_executable(PgDemo
        internal/db/main.cpp
)
target_link_libraries(PgDemo PRIVATE storage)