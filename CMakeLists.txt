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
        internal/lib/spin_lock/spin_lock.h
        internal/lib/spin_lock/spin_lock.cpp
        internal/lib/semaphore/semaphore.h
        internal/lib/semaphore/semahpore.cpp
)
