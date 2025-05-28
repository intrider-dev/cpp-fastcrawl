// src/HttpClient.cpp
#include "HttpClient.hpp"
#include "Logger.hpp"
#include "Worker.hpp"

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

bool HttpClient::fetchAndWrite(const std::string& url, FILE* outfp) {
    if (!curlHandle) return false;
    curl_easy_setopt(curlHandle, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curlHandle, CURLOPT_NOBODY, 1L);
    curl_easy_setopt(curlHandle, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curlHandle, CURLOPT_TIMEOUT, 10L);

    CURLcode res = curl_easy_perform(curlHandle);
    if (res != CURLE_OK) {
        Logger::error("CURL error for %s: %s", url.c_str(), curl_easy_strerror(res));
        return false;
    }
    long code = 0;
    curl_easy_getinfo(curlHandle, CURLINFO_RESPONSE_CODE, &code);
    if (code == 200) {
        std::lock_guard<std::mutex> lock(Worker::outputMutex);
        std::fprintf(outfp, "%s\n", url.c_str());
        std::fflush(outfp);
        return true;
    }
    return false;
}
