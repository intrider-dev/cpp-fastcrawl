#include "HttpClient.hpp"
#include "Worker.hpp"
#include <thread>
#include <curl/curl.h>
#include <algorithm>

HttpClient::HttpClient(size_t threadCount, size_t initialRequestsPerThread,
                       FILE* outfp, std::mutex* fileMutex, std::atomic<size_t>* matched)
  : threadCount_(threadCount)
  , initialRequestsPerThread_(initialRequestsPerThread)
  , outfp_(outfp)
  , fileMutex_(fileMutex)
  , matched_(matched)
{
    curl_global_init(CURL_GLOBAL_ALL);
}

HttpClient::~HttpClient() {
    curl_global_cleanup();
}

void HttpClient::fetchAll(const std::vector<std::string>& urls,
                          const std::vector<std::string>& keywords)
{
    std::atomic<size_t> nextIndex{0};

    std::vector<Worker> workers;
    workers.reserve(threadCount_);
    std::vector<std::thread> threads;
    threads.reserve(threadCount_);

    for (size_t i = 0; i < threadCount_; ++i) {
        workers.emplace_back(
            urls, nextIndex, keywords, initialRequestsPerThread_,
            outfp_, fileMutex_, matched_
        );
        threads.emplace_back(std::ref(workers.back()));
    }

    for (auto& t : threads)
        if (t.joinable()) t.join();
}
