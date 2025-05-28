// src/HttpClient.cpp
#include "HttpClient.hpp"
#include "Logger.hpp"
#include <cstdio>

HttpClient::HttpClient() {
    curlHandle = curl_easy_init();
    if (!curlHandle) {
        Logger::error("CURL initialization failed");
    }
}

HttpClient::~HttpClient() {
    if (curlHandle)
        curl_easy_cleanup(curlHandle);
}

size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t totalSize = size * nmemb;
    std::string* str = static_cast<std::string*>(userp);
    str->append(static_cast<char*>(contents), totalSize);
    return totalSize;
}

size_t WriteToStringCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t totalSize = size * nmemb;
    std::string* str = static_cast<std::string*>(userp);
    str->append(static_cast<char*>(contents), totalSize);
    return totalSize;
}

bool HttpClient::fetch(const std::string& url, std::string& body) {
    if (!curlHandle) return false;

    body.clear();

    curl_easy_setopt(curlHandle, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curlHandle, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curlHandle, CURLOPT_TIMEOUT, 10L);

    // Убираем CURLOPT_NOBODY — нам нужен полный ответ с телом
    curl_easy_setopt(curlHandle, CURLOPT_NOBODY, 0L);

    curl_easy_setopt(curlHandle, CURLOPT_WRITEFUNCTION, WriteToStringCallback);
    curl_easy_setopt(curlHandle, CURLOPT_WRITEDATA, &body);

    CURLcode res = curl_easy_perform(curlHandle);
    if (res != CURLE_OK) {
        Logger::error("CURL error for %s: %s", url.c_str(), curl_easy_strerror(res));
        return false;
    }
    return true;
}

size_t WriteToFileCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    FILE* fp = static_cast<FILE*>(userp);
    size_t written = fwrite(contents, size, nmemb, fp);
    return written;
}

bool HttpClient::fetchToFile(const std::string& url, FILE* outfp) {
    if (!curlHandle || !outfp)
        return false;

    curl_easy_setopt(curlHandle, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curlHandle, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curlHandle, CURLOPT_WRITEFUNCTION, WriteToFileCallback);
    curl_easy_setopt(curlHandle, CURLOPT_WRITEDATA, outfp);
    curl_easy_setopt(curlHandle, CURLOPT_TIMEOUT, 10L);

    CURLcode res = curl_easy_perform(curlHandle);
    if (res != CURLE_OK) {
        Logger::error("CURL error for %s: %s", url.c_str(), curl_easy_strerror(res));
        return false;
    }
    long code = 0;
    curl_easy_getinfo(curlHandle, CURLINFO_RESPONSE_CODE, &code);
    if (code != 200) {
        Logger::error("HTTP response code %ld for %s", code, url.c_str());
        return false;
    }
    return true;
}
