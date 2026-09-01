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

# ---- Учебный Telegram-бот: печатает запрос и отвечает текущим временем ----
add_executable(TelegramBot
        internal/telegram_bot/main.cpp
        internal/telegram_bot/bot.cpp
)
target_link_libraries(TelegramBot PRIVATE cpr::cpr nlohmann_json::nlohmann_json)

add_executable(DevOpsBot
        internal/status_checker/main.cpp
        internal/status_checker/checkers/builder/builder.cpp
        internal/status_checker/checkers/http/checker.cpp
        internal/status_checker/checkers/https/checker.cpp
        internal/lib/thread_pools/thread_pool.h
        internal/lib/thread_pools/thread_pool.cpp
        internal/lib/thread_queue/thread_queue.h
        internal/lib/thread_queue/thread_queue.cpp
)
target_link_libraries(DevOpsBot PRIVATE cpr::cpr)

# ---- Юнит-тесты на внутренности HTTP-чекера ----
# fetcher.cpp сюда НЕ добавляем: тесты подменяют сеть своим MockFetcher.
enable_testing()
include(GoogleTest)

add_executable(checker_test
        internal/status_checker/checkers/http/checker_test.cpp
        internal/status_checker/checkers/http/checker.cpp
        ../../../../../../wsl.localhost/Ubuntu/home/idk/projects/DevOpsBot/internal/db/main.cpp
        ../../../../../../wsl.localhost/Ubuntu/home/idk/projects/DevOpsBot/src/storage.cpp
        ../../../../../../wsl.localhost/Ubuntu/home/idk/projects/DevOpsBot/internal/db/storage.cpp
)
target_link_libraries(checker_test PRIVATE gtest_main cpr::cpr)
gtest_discover_tests(checker_test)


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

# ---- Демонстрация записи событий бота в PostgreSQL ----
add_executable(PgDemo
        internal/db/main.cpp
        internal/db/storage.cpp
)
target_include_directories(PgDemo PRIVATE internal/db)
target_link_libraries(PgDemo PRIVATE libpqxx::pqxx)