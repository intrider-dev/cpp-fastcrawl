cmake_minimum_required(VERSION 3.12)
project(HighPerfCrawler LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/CMakeModules")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Опции: можно переключиться на C++20 при необходимости
# set(CMAKE_CXX_STANDARD 20)

# Поиск libcurl
find_package(CURL REQUIRED)

# Потоки (на Unix будет линкуется pthread, на Windows ничего не нужно)
find_package(Threads REQUIRED)

# Исполняемый файл и исходники
add_executable(${PROJECT_NAME}
        src/main.cpp
        src/DomainLoader.cpp
        src/HttpClient.cpp
        src/Worker.cpp
        src/Logger.cpp       # <-- вот эта строка
)

# Заголовочные директории
target_include_directories(${PROJECT_NAME}
        PRIVATE
        ${CURL_INCLUDE_DIRS}
        src
)

# Линковка
target_link_libraries(${PROJECT_NAME}
        PRIVATE
        CURL::libcurl
        Threads::Threads
)

# Опционально: добавить флаг для debug-режима
option(ENABLE_DEBUG "Enable debug logging" OFF)
if(ENABLE_DEBUG)
    target_compile_definitions(${PROJECT_NAME} PRIVATE DEBUG_MODE)
endif()

# Вывод пути к curl для отладки
message(STATUS "CURL include dirs: ${CURL_INCLUDE_DIRS}")
message(STATUS "CURL libraries: ${CURL_LIBRARIES}")
