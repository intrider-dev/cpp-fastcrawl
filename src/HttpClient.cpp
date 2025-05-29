#include "HttpClient.hpp"
#include "Worker.hpp"
#include <thread>
#include <curl/curl.h>
#include <algorithm>
#include <ostream>
#include <fstream>

HttpClient::HttpClient(size_t threadCount,
                       size_t initialRequestsPerThread,
                       FILE* outfp,
                       std::mutex* fileMutex,
                       std::atomic<size_t>* matched,
                       std::atomic<size_t>* resumeIndex)
  : threadCount_(threadCount)
  , initialRequestsPerThread_(initialRequestsPerThread)
  , outfp_(outfp)
  , fileMutex_(fileMutex)
  , matched_(matched)
  , resumeIndex_(resumeIndex)
{
    curl_global_init(CURL_GLOBAL_ALL);
}

HttpClient::~HttpClient() {
    curl_global_cleanup();
}

void HttpClient::fetchAll(const std::vector<std::string>& urls,
                          const std::vector<std::string>& keywords)
{
    // Используем глобальный указатель на атомарный индекс
    std::atomic<size_t>* nextIndex = resumeIndex_;

    std::vector<Worker> workers;
    workers.reserve(threadCount_);
    std::vector<std::thread> threads;
    threads.reserve(threadCount_);

    for (size_t i = 0; i < threadCount_; ++i) {
        workers.emplace_back(
            urls,                 // список доменов
            nextIndex,            // указатель на общий индекс
            keywords,             // ключевые слова
            initialRequestsPerThread_,
            outfp_,               // FILE*
            fileMutex_,           // мьютекс для логов
            matched_              // счётчик совпадений
        );
        threads.emplace_back(std::ref(workers.back()));
    }

    for (auto& t : threads) {
        if (t.joinable()) t.join();
    }

    // Сохраняем значение после завершения всех потоков
    if (!checkpointFile_.empty() && resumeIndex_) {
        std::ofstream out(checkpointFile_, std::ios::trunc);
        if (out.is_open()) {
            out << resumeIndex_->load() << std::endl;
            out.close();
        }
    }
}

void HttpClient::setResumeIndex(size_t index) {
    if (resumeIndex_) {
        resumeIndex_->store(index);
    }
}

void HttpClient::setCheckpointFile(const std::string& path) {
    checkpointFile_ = path;
}

