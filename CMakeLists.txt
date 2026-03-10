cmake_minimum_required(VERSION 3.15)
project(DevOpsBot VERSION 1.0.0 LANGUAGES CXX)

# Используем C++17
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Создаем исполняемый файл
add_executable(devopsbot src/main.cpp)

# Подключаем заголовочные файлы
target_include_directories(devopsbot PRIVATE ${CMAKE_SOURCE_DIR}/include)

# Базовые настройки компилятора
if(MSVC)
    target_compile_options(devopsbot PRIVATE /W4)
else()
    target_compile_options(devopsbot PRIVATE -Wall -Wextra)
endif()
