cmake_minimum_required(VERSION 3.15)
project(DevOpsBot VERSION 1.0.0 LANGUAGES CXX)

# Используем C++17
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Создаем исполняемый файл
add_executable(devopsbot src/main.cpp
        libs/unique_ptr/uniqueptr_test.cpp)

# Подключаем заголовочные файлы
target_include_directories(devopsbot PRIVATE ${CMAKE_SOURCE_DIR}/include)

# Базовые настройки компилятора
if(MSVC)
    target_compile_options(devopsbot PRIVATE /W4)
else()
    target_compile_options(devopsbot PRIVATE -Wall -Wextra)
endif()

# GoogleTest
include(FetchContent)
FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.15.2
)
FetchContent_MakeAvailable(googletest)

enable_testing()

add_executable(
        uniqueptr_test libs/unique_ptr/UniquePtr_test.cpp
)
target_include_directories(uniqueptr_test PRIVATE ${CMAKE_SOURCE_DIR}/cmake)
target_link_libraries(uniqueptr_test GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(uniqueptr_test)