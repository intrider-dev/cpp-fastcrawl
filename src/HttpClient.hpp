// src/HttpClient.hpp
#ifndef HTTPCLIENT_HPP
#define HTTPCLIENT_HPP

#include <string>
#include <curl/curl.h>
#include <cstdio>

class HttpClient {
public:
    HttpClient();
    ~HttpClient();

    bool fetchAndWrite(const std::string& url, FILE* outfp);

private:
    CURL* curlHandle;
};

#endif // HTTPCLIENT_HPP
