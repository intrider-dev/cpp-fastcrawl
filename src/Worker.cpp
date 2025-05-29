#include "Worker.hpp"
#include "CommonTypes.hpp"
#include "Logger.hpp"
#include <curl/curl.h>
#include <unordered_map>
#include <mutex>
#include <thread>
#include <condition_variable>

// --- глобальный share-handle для DNS кеша ---
static CURLSH*     g_shareHandle = nullptr;
static std::once_flag g_shareFlag;

// инициализация DNS-кеша (один раз на процесс)
void Worker::initCurlShare() {
    std::call_once(g_shareFlag, [](){
        g_shareHandle = curl_share_init();
        if (g_shareHandle)
            curl_share_setopt(g_shareHandle, CURLSHOPT_SHARE, CURL_LOCK_DATA_DNS);
    });
}

// записываем кусок тела в EasyContext.html
size_t Worker::WriteCallback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* ctx = static_cast<EasyContext*>(userdata);
    ctx->html.append(ptr, size * nmemb);
    return size * nmemb;
}

Worker::Worker(const std::vector<std::string>& urls,
               std::atomic<size_t>& nextIndex,
               const std::vector<std::string>& keywords,
               size_t initialRequests,
               FILE* out,
               std::mutex* fileMutex)
    : urls_(urls), nextIndex_(nextIndex), keywords_(keywords),
      initialRequests_(initialRequests), out_(out), fileMutex_(fileMutex)
{}
void Worker::operator()() {
    // убедимся, что DNS-кеш готов
    initCurlShare();

    // создаём мульти-хэндл
    CURLM* multi = curl_multi_init();
    if (!multi) {
        Logger::warn("Worker: curl_multi_init failed");
        return;
    }
    // рекомендуемые опции
    curl_multi_setopt(multi, CURLMOPT_MAX_TOTAL_CONNECTIONS, 500000L);
    curl_multi_setopt(multi, CURLMOPT_PIPELINING,         CURLPIPE_MULTIPLEX);

    int stillRunning = 0;
    std::unordered_map<CURL*, EasyContext*> ctxMap;

    // лямбда для старта одного запроса
    auto launch = [&](size_t idx){
        const auto& url = urls_[idx];
        CURL* easy = curl_easy_init();
        if (!easy) {
            Logger::warn("Worker: curl_easy_init failed for %s", url.c_str());
            return;
        }
        auto* ctx = new EasyContext{url, "", 0L};

        curl_easy_setopt(easy, CURLOPT_URL,            ctx->url.c_str());
        curl_easy_setopt(easy, CURLOPT_WRITEFUNCTION,  WriteCallback);
        curl_easy_setopt(easy, CURLOPT_WRITEDATA,      ctx);
        curl_easy_setopt(easy, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(easy, CURLOPT_CONNECTTIMEOUT, 30L);
        curl_easy_setopt(easy, CURLOPT_TIMEOUT,        20L);
        curl_easy_setopt(easy, CURLOPT_LOW_SPEED_LIMIT,10L);
        curl_easy_setopt(easy, CURLOPT_LOW_SPEED_TIME, 3L);
        curl_easy_setopt(easy, CURLOPT_NOSIGNAL,       1L);
        curl_easy_setopt(easy, CURLOPT_HTTP_VERSION,   CURL_HTTP_VERSION_2TLS);
        curl_easy_setopt(easy, CURLOPT_PIPEWAIT,       1L);
        if (g_shareHandle)
            curl_easy_setopt(easy, CURLOPT_SHARE, g_shareHandle);

        // буфер для ошибок
        char* errbuf = (char*)malloc(CURL_ERROR_SIZE);
        if (errbuf) {
            errbuf[0] = '\0';
            curl_easy_setopt(easy, CURLOPT_ERRORBUFFER, errbuf);
        }

        CURLMcode mc = curl_multi_add_handle(multi, easy);
        if (mc != CURLM_OK) {
            Logger::warn("Worker: add_handle failed for %s: %s",
                         url.c_str(), curl_multi_strerror(mc));
            curl_easy_cleanup(easy);
            delete ctx;
            free(errbuf);
            return;
        }
        ctxMap[easy] = ctx;
    };

    // стартуем initialRequests_ первых URL
    for (size_t i = 0; i < initialRequests_; ++i) {
        size_t idx = nextIndex_++;
        if (idx >= urls_.size()) break;
        launch(idx);
    }

    // запускаем
    curl_multi_perform(multi, &stillRunning);

    // основной цикл
    while (stillRunning > 0) {
        curl_multi_poll(multi, nullptr, 0, 1000, nullptr);
        curl_multi_perform(multi, &stillRunning);

        int msgsLeft = 0;
        while (auto* msg = curl_multi_info_read(multi, &msgsLeft)) {
            if (msg->msg != CURLMSG_DONE) continue;
            CURL* easy = msg->easy_handle;
            auto it = ctxMap.find(easy);
            if (it == ctxMap.end()) {
                curl_multi_remove_handle(multi, easy);
                curl_easy_cleanup(easy);
                continue;
            }
            auto* ctx = it->second;
            curl_easy_getinfo(easy, CURLINFO_RESPONSE_CODE, &ctx->statusCode);

            // обрабатываем
            processFinished(ctx->url, ctx->html, ctx->statusCode);

            // убираем хэндл
            curl_multi_remove_handle(multi, easy);
            curl_easy_cleanup(easy);
            delete ctx;
            ctxMap.erase(it);

            // запускаем следующий
            size_t idx = nextIndex_++;
            if (idx < urls_.size()) launch(idx);
        }
    }

    // чистим остатки
    for (auto& kv : ctxMap) {
        curl_multi_remove_handle(multi, kv.first);
        curl_easy_cleanup(kv.first);
        delete kv.second;
    }
    curl_multi_cleanup(multi);
}

void Worker::processFinished(const std::string& url,
                             const std::string& html,
                             long statusCode)
{
    bool found = false;
    for (const auto& kw : keywords_) {
        if (html.find(kw) != std::string::npos) {
            found = true;
            break;
        }
    }
    if (Logger::isDebug()) {
        Logger::debug("Worker: %s [status=%ld] [%s]",
                      url.c_str(), statusCode, found ? "match" : "no match");
    }
    // Потоковая запись: только если fileMutex_ и out_ заданы
    if (out_ && fileMutex_ && statusCode == 200 && (keywords_.empty() || found)) {
        std::string d = url;
        if (auto p = d.find("://"); p != std::string::npos)
            d = d.substr(p + 3);
        std::lock_guard<std::mutex> lock(*fileMutex_);
        std::fprintf(out_, "%s\n", d.c_str());
        std::fflush(out_);
    }
    // Для статистики (если нужно собрать результаты)
    HttpResponse resp{url, html, statusCode, found};
    results_.push_back(std::move(resp));
}

