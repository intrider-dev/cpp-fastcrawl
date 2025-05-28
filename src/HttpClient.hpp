// src/HttpClient.hpp
#ifndef HTTPCLIENT_HPP
#define HTTPCLIENT_HPP

#include <string>
#include <curl/curl.h>

class HttpClient {
public:
    HttpClient();
    ~HttpClient();

    bool fetch(const std::string& url, std::string& responseBody);
    bool fetchToFile(const std::string& url, FILE* outfp);

private:
    CURL* curlHandle;
    static size_t writeCallback(void* contents, size_t size, size_t nmemb, void* userp);
};

#endif // HTTPCLIENT_HPP
