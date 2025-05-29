cmake_minimum_required(VERSION 3.12)
project(HighPerfCrawler LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Путь к пользовательским модулям CMake
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/CMakeModules")

find_package(CURL REQUIRED)
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)

option(ENABLE_DEBUG "Enable debug logging" OFF)
if(ENABLE_DEBUG)
    add_compile_definitions(DEBUG_MODE)
endif()

# Основной исполняемый файл
add_executable(${PROJECT_NAME}
        src/main.cpp
        src/DomainLoader.cpp
        src/HttpClient.cpp
        src/Worker.cpp
        src/Logger.cpp
        src/CurlShare.cpp
)

target_include_directories(${PROJECT_NAME}
        PRIVATE
        ${CURL_INCLUDE_DIRS}
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(${PROJECT_NAME}
        PRIVATE
        CURL::libcurl
        Threads::Threads
        OpenSSL::Crypto
)

# Отладочный вывод путей
message(STATUS "CURL include dirs: ${CURL_INCLUDE_DIRS}")
message(STATUS "CURL libraries: ${CURL_LIBRARIES}")

# Дополнительный исполняемый файл: launcher
add_executable(launcher src/main_launcher.cpp)

# Он не зависит от libcurl/OpenSSL и не ссылается на основную логику
target_link_libraries(launcher PRIVATE Threads::Threads)
