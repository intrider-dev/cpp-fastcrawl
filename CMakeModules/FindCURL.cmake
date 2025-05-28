# CMakeModules/FindLibCurl.cmake
# Поиск библиотеки libcurl и заголовков

# Имя переменных:
#   CURL_FOUND       - булева переменная: TRUE, если libcurl найдена
#   CURL_INCLUDE_DIR - путь к каталогу, содержащему curl/curl.h
#   CURL_LIBRARY     - путь к библиотеке libcurl (libcurl.so, libcurl.a, или curl.lib)

find_path(CURL_INCLUDE_DIR
        NAMES curl/curl.h
        HINTS
        ${CMAKE_PREFIX_PATH}
        /usr/include
        /usr/local/include
)

find_library(CURL_LIBRARY
        NAMES curl libcurl
        HINTS
        ${CMAKE_PREFIX_PATH}
        /usr/lib
        /usr/local/lib
        /usr/lib/x86_64-linux-gnu
)

if(CURL_INCLUDE_DIR AND CURL_LIBRARY)
    set(CURL_FOUND TRUE)
    # Создаём импортированную цель CURL::libcurl
    add_library(CURL::libcurl UNKNOWN IMPORTED)
    set_target_properties(CURL::libcurl PROPERTIES
            IMPORTED_LOCATION "${CURL_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${CURL_INCLUDE_DIR}"
    )
    set(CURL_FOUND TRUE)
else()
    set(CURL_FOUND FALSE)
endif()

mark_as_advanced(CURL_INCLUDE_DIR CURL_LIBRARY)

if(NOT CURL_FOUND)
    message(FATAL_ERROR "libcurl not found. Please install libcurl development package or set CURL_INCLUDE_DIR and CURL_LIBRARY manually.")
endif()
