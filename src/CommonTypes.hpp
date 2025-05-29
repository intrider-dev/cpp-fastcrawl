#pragma once
#include <string>

struct HttpResponse {
    std::string url;
    std::string html;
    long        statusCode;
    bool        keywordFound = false;
};

struct EasyContext {
    std::string url;
    std::string html;
    long        statusCode = 0;
};
