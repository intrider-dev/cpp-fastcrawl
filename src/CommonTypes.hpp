// CommonTypes.hpp
#pragma once

#include <string>

struct HttpResponse {
    std::string url;
    std::string html;
    long statusCode;
    bool keywordFound = false;
};

struct EasyContext {
    std::string url;
    std::string html;
    long statusCode = 0;
    char* curlErr = nullptr;
};

struct Args {
    std::string input;
    std::string output;
    int threads = 0;
    std::vector<std::string> contains;
    bool debug = false;
};
