#include "HttpClient.hpp"
#include "Worker.hpp"
#include <atomic>
#include <thread>
#include <mutex>
#include <cstdio>
#include <curl/curl.h>

HttpClient::HttpClient(size_t threadCount, size_t initialRequestsPerThread)
    : threadCount_(threadCount), initialRequestsPerThread_(initialRequestsPerThread)
{
    curl_global_init(CURL_GLOBAL_ALL);
}

HttpClient::~HttpClient() {
    curl_global_cleanup();
}

// ПОТОКОВАЯ запись — outfp и fileMutex передаются во все воркеры
std::vector<HttpResponse>
HttpClient::fetchAll(const std::vector<std::string>& urls,
                     const std::vector<std::string>& keywords,
                     FILE* outfp,
                     std::mutex* fileMutex)
{
    std::atomic<size_t> nextIndex(0);
    const size_t totalUrls = urls.size();

    std::vector<Worker> workers;
    workers.reserve(threadCount_);
    std::vector<std::thread> threads;
    threads.reserve(threadCount_);

    for (size_t i = 0; i < threadCount_; ++i) {
        workers.emplace_back(urls, nextIndex, keywords, initialRequestsPerThread_, outfp, fileMutex);
        threads.emplace_back(std::ref(workers.back()));
    }
    // КЛЮЧЕВОЙ МОМЕНТ: Ожидаем завершения ВСЕХ потоков ДО закрытия outfp
    for (std::thread &t : threads) {
        if (t.joinable()) t.join();
    }
    // Здесь уже можно безопасно закрывать outfp (но делается это ВНЕ класса, в main.cpp!)

    // Сбор всех результатов (для статистики, если нужно)
    std::vector<HttpResponse> allResults;
    allResults.reserve(totalUrls);
    for (Worker &w : workers) {
        const std::vector<HttpResponse>& part = w.getResults();
        allResults.insert(allResults.end(), part.begin(), part.end());
    }
    return allResults;
}
