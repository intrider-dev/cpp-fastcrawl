cmake_minimum_required(VERSION 3.12)
project(HighPerfCrawler LANGUAGES CXX)

# Путь к пользовательским модулям CMake (например, FindLibCurl.cmake)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/CMakeModules")

# Установка стандарта C++
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Поиск библиотеки libcurl (если не найдена системная, используется FindLibCurl.cmake)
find_package(CURL REQUIRED)

# Потоки (на Linux подключает pthread, на Windows — ничего)
find_package(Threads REQUIRED)

# Опционально: добавить флаг компиляции для включения отладочной информации
option(ENABLE_DEBUG "Enable debug logging" OFF)
if(ENABLE_DEBUG)
    add_compile_definitions(DEBUG_MODE)
endif()

# Исполняемый файл и исходники
add_executable(${PROJECT_NAME}
        src/main.cpp
        src/DomainLoader.cpp
        src/HttpClient.cpp
        src/Worker.cpp
        src/Logger.cpp
        src/CurlShare.cpp
)

# Подключаемые директории с заголовками
target_include_directories(${PROJECT_NAME}
        PRIVATE
        ${CURL_INCLUDE_DIRS}
        ${CMAKE_SOURCE_DIR}/src
)

# Линковка с нужными библиотеками
target_link_libraries(${PROJECT_NAME}
        PRIVATE
        CURL::libcurl
        Threads::Threads
)

# Для отладки — вывод путей к curl
message(STATUS "CURL include dirs: ${CURL_INCLUDE_DIRS}")
message(STATUS "CURL libraries: ${CURL_LIBRARIES}")
