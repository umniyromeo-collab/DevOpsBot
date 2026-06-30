cmake_minimum_required(VERSION 3.15)
project(DevOpsBot)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(internal/status_checker/checkers)

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

# ---- Юнит-тесты на GoogleTest ----
include(FetchContent)
FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        v1.15.2
)
FetchContent_MakeAvailable(googletest)

enable_testing()
include(GoogleTest)

# Red-слой: тесты на внутренности HTTP-чекера.
# Сейчас не компилируются — у Checker ещё нет конструктора с IFetcher.
add_executable(checker_test
        internal/status_checker/checkers/http/checker_test.cpp
        internal/status_checker/checkers/http/checker.cpp
        internal/status_checker/checkers/http/fetcher.h
        internal/status_checker/checkers/http/fetcher.cpp
)
target_link_libraries(checker_test PRIVATE gtest_main)
gtest_discover_tests(checker_test)

find_package(CURL REQUIRED)
target_link_libraries(DevOpsBot PRIVATE CURL::libcurl cpr::cpr)
