#pragma once

#include <vector>
#include <string>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <curl/curl.h>
#include <sys/epoll.h>
#include "CommonTypes.hpp"

class Worker {
    FILE* outfp_;
    std::mutex* fileMutex_;
public:
    Worker(const std::vector<std::string>& urls,
       std::atomic<size_t>& nextIndex,
       const std::vector<std::string>& keywords,
       size_t initialRequests,
       FILE* outfp,
       std::mutex* fileMutex,
       std::atomic<size_t>* matched);

    // Entry point для std::thread
    void operator()();

    // Результаты после завершения
    const std::vector<HttpResponse>& getResults() const { return results_; }

    // Коллбэки libcurl
    static size_t writeCallback(char* ptr, size_t size, size_t nmemb, void* userdata);
    static int    sockCb(CURL* easy, curl_socket_t s, int what, void* userp, void* socketp);
    static int    curlSocketCallback(CURL* easy, curl_socket_t s, int what, void* userp, void* sockp);
    static int    timerCb(CURLM* multi, long timeout_ms, void* userp);

private:
    // Вспомогалки
    void addHandle(size_t idx);   // запускаем новый easy
    void removeHandle(CURL* easy);
    void safeRemove(CURL* easy);

    // Данные, переданные из HttpClient
    const std::vector<std::string>& urls_;
    std::atomic<size_t>&            nextIndex_;
    const std::vector<std::string>& keywords_;
    size_t                          initialRequests_;
    std::atomic<size_t>* matched_;

    // curl-multi, epoll и связанные мапы
    CURLM*                                       multi_;
    int                                          epfd_;
    long                                         timer_ms_;  // таймаут из timerCb
    std::unordered_map<curl_socket_t, uint32_t>  sockFlags_;
    std::unordered_map<CURL*, EasyContext*>      ctxMap_;

    // Накопленные результаты
    std::vector<HttpResponse> results_;
};
