#pragma once
#include <mutex>
#include <vector>
#include <string>
#include <thread>
#include "CommonTypes.hpp"

class HttpClient {
public:
    // threads = cpu-1 по умолчанию, parallel per thread = 100
    HttpClient(size_t threads =
                  (std::thread::hardware_concurrency()>1?
                   std::thread::hardware_concurrency()-1:1),
               size_t initialPerThread = 100);
    ~HttpClient();

    // fetchAll вернёт все HttpResponse
    std::vector<HttpResponse> fetchAll(const std::vector<std::string>& urls,
                                       const std::vector<std::string>& keywords,
                                       FILE* outfp = nullptr,
                                       std::mutex* fileMutex = nullptr);

private:
    size_t threadCount_;
    size_t initialRequestsPerThread_;
};
