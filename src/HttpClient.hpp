#pragma once

#include "Worker.hpp"
#include <vector>
#include <string>
#include <mutex>
#include <atomic>
#include <cstdio>

class HttpClient {
public:
    HttpClient(size_t threadCount, size_t initialRequestsPerThread,
               FILE* outfp, std::mutex* fileMutex, std::atomic<size_t>* matched);
    ~HttpClient();

    // Теперь ничего не возвращает: всё пишется сразу, счётчик matched обновляется на лету.
    void fetchAll(const std::vector<std::string>& urls,
                  const std::vector<std::string>& keywords);

private:
    size_t threadCount_;
    size_t initialRequestsPerThread_;
    FILE* outfp_;
    std::mutex* fileMutex_;
    std::atomic<size_t>* matched_;
};
