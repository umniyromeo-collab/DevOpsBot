cmake_minimum_required(VERSION 3.15)
project(DevOpsBot)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(internal/status_checker/checkers)

add_executable(DevOpsBot
        main.cpp
        internal/status_checker/checkers/builder/builder.cpp
        internal/status_checker/checkers/http/checker.cpp
        internal/status_checker/checkers/https/checker.cpp
)
