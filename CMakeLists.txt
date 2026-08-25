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
)
target_link_libraries(checker_test PRIVATE gtest_main cpr::cpr)
gtest_discover_tests(checker_test)
