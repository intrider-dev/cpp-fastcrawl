// src/HttpClient.hpp
#ifndef HTTPCLIENT_HPP
#define HTTPCLIENT_HPP

#include <string>
#include <vector>
#include <curl/curl.h>

struct HttpResponse {
    std::string url;
    std::string headers;
    std::string body;
    long        code = 0; // HTTP response code (например, 200, 404, 301, ...), по умолчанию 0
};

class HttpClient {
public:
    HttpClient();
    ~HttpClient();

    // Синхронная загрузка (обычно используется для отладки)
    bool fetch(const std::string& url, std::string& responseBody);

    // Синхронная загрузка в файл
    bool fetchToFile(const std::string& url, FILE* outfp);

    // Асинхронная загрузка одного URL (на самом деле делает запрос в фоне через curl_multi)
    bool fetchAsync(const std::string& url, std::string& responseBody);

    // Множественная асинхронная загрузка (гарантирует возврат даже при ошибках)
    std::vector<HttpResponse> fetchMulti(const std::vector<std::string>& urls);

    // Позволяет использовать writeCallback как нестатическую функцию (опционально)
    static size_t writeCallback(char* ptr, size_t size, size_t nmemb, void* userdata);

    // Позволяет использовать headerCallback как нестатическую функцию (опционально)
    static size_t headerCallback(char* ptr, size_t size, size_t nmemb, void* userdata);

private:
    CURL* curlHandle = nullptr; // Используется только для одиночных fetch
};

#endif // HTTPCLIENT_HPP
