// Worker.hpp
#pragma once
#include <vector>
#include <atomic>
#include <string>
#include <mutex>
#include <cstdio>
#include "CommonTypes.hpp"

class Worker {
    FILE* out_ = nullptr;
    std::mutex* fileMutex_ = nullptr;
public:
    Worker(const std::vector<std::string>& urls,
           std::atomic<size_t>& nextIndex,
           const std::vector<std::string>& keywords,
           size_t initialRequests,
           FILE* out = nullptr,
           std::mutex* fileMutex = nullptr);

    void operator()();

    const std::vector<HttpResponse>& getResults() const { return results_; }
    static size_t WriteCallback(char* ptr, size_t size, size_t nmemb, void* userdata);
    static void initCurlShare();

private:
    void processFinished(const std::string& url, const std::string& html, long statusCode);

    const std::vector<std::string>& urls_;
    std::atomic<size_t>& nextIndex_;
    const std::vector<std::string>& keywords_;
    size_t initialRequests_;
    std::vector<HttpResponse> results_;
};
