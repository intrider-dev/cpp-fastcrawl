#include "Worker.hpp"
#include "Logger.hpp"

#include <sys/epoll.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <mutex>
#include <cassert>
#include <fstream>
#include <memory>

// Один share-handle на весь процесс
static CURLSH*      g_shareHandle = nullptr;
static std::once_flag g_shareFlag;

// Инициализируем общий DNS-кеш
void Worker::initCurlShare() {
    std::call_once(g_shareFlag, []() {
        g_shareHandle = curl_share_init();
        if (!g_shareHandle) {
            Logger::warn("Worker: curl_share_init failed");
            return;
        }
        curl_share_setopt(g_shareHandle, CURLSHOPT_SHARE, CURL_LOCK_DATA_DNS);
    });
}

Worker::Worker(const std::vector<std::string>& urls,
               std::atomic<size_t>& nextIndex,
               const std::vector<std::string>& keywords,
               size_t initialRequests,
               FILE* outfp,
               std::mutex* fileMutex,
               std::atomic<size_t>* matched)
  : urls_(urls)
  , nextIndex_(nextIndex)
  , keywords_(keywords)
  , initialRequests_(initialRequests)
  , multi_(nullptr)
  , epfd_(-1)
  , timer_ms_(-1)
  , outfp_(outfp)
  , fileMutex_(fileMutex)
  , matched_(matched)
{}

size_t Worker::writeCallback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* ctx = static_cast<EasyContext*>(userdata);
    ctx->html.append(ptr, size * nmemb);
    return size * nmemb;
}

int Worker::curlSocketCallback(CURL* easy, curl_socket_t s, int what, void* userp, void* sockp) {
    auto* self = static_cast<Worker*>(userp);
    struct epoll_event ev;
    ev.data.fd = s;

    uint32_t events = 0;
    switch (what) {
        case CURL_POLL_IN:  events = EPOLLIN; break;
        case CURL_POLL_OUT: events = EPOLLOUT; break;
        case CURL_POLL_INOUT: events = EPOLLIN | EPOLLOUT; break;
        case CURL_POLL_REMOVE:
            epoll_ctl(self->epfd_, EPOLL_CTL_DEL, s, nullptr);
            self->sockFlags_.erase(s);
            return 0;
        default: return 0;
    }

    ev.events = events | EPOLLET;

    auto it = self->sockFlags_.find(s);
    if (it == self->sockFlags_.end()) {
        // сокет не зарегистрирован — добавляем
        if (epoll_ctl(self->epfd_, EPOLL_CTL_ADD, s, &ev) == -1) {
            Logger::warn("Worker: epoll_ctl ADD failed: %s", strerror(errno));
        } else {
            self->sockFlags_[s] = events;
        }
    } else {
        // сокет уже есть — модифицируем
        if (epoll_ctl(self->epfd_, EPOLL_CTL_MOD, s, &ev) == -1) {
            Logger::warn("Worker: epoll_ctl MOD failed: %s", strerror(errno));
        } else {
            self->sockFlags_[s] = events;
        }
    }

    curl_multi_assign(self->multi_, s, self);
    return 0;
}

int Worker::timerCb(CURLM* /*multi*/, long timeout_ms, void* userp) {
    Worker* self = static_cast<Worker*>(userp);
    self->timer_ms_ = timeout_ms;
    return 0;
}

void Worker::addHandle(size_t idx) {
    const std::string& url = urls_[idx];
    CURL* easy = curl_easy_init();
    if (!easy) {
        Logger::warn("Worker: curl_easy_init failed for %s", url.c_str());
        return;
    }
    auto* ctx = new EasyContext{url, "", 0L};
    ctxMap_[easy] = ctx;

    curl_easy_setopt(easy, CURLOPT_URL,            url.c_str());
    curl_easy_setopt(easy, CURLOPT_WRITEFUNCTION,  writeCallback);
    curl_easy_setopt(easy, CURLOPT_WRITEDATA,      ctx);
    curl_easy_setopt(easy, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(easy, CURLOPT_TIMEOUT,        20L);
    curl_easy_setopt(easy, CURLOPT_NOSIGNAL,       1L);
    if (g_shareHandle) // общий DNS
        curl_easy_setopt(easy, CURLOPT_SHARE, g_shareHandle);

    curl_multi_add_handle(multi_, easy);
}

void Worker::removeHandle(CURL* easy) {
    curl_multi_remove_handle(multi_, easy);
    curl_easy_cleanup(easy);
}

void Worker::operator()() {
    initCurlShare();

    // 1) создаём multi
    multi_ = curl_multi_init();
    if (!multi_) {
        Logger::warn("Worker: curl_multi_init failed");
        return;
    }
    curl_multi_setopt(multi_, CURLMOPT_PIPELINING,      CURLPIPE_MULTIPLEX);
    curl_multi_setopt(multi_, CURLMOPT_SOCKETFUNCTION,  curlSocketCallback);
    curl_multi_setopt(multi_, CURLMOPT_SOCKETDATA,      this);
    curl_multi_setopt(multi_, CURLMOPT_TIMERFUNCTION,   timerCb);
    curl_multi_setopt(multi_, CURLMOPT_TIMERDATA,       this);

    // 2) создаём epoll
    epfd_ = epoll_create1(0);
    if (epfd_ < 0) {
        Logger::warn("Worker: epoll_create1 failed: %s", std::strerror(errno));
        curl_multi_cleanup(multi_);
        return;
    }

    // 3) стартовый «пул» запросов
    for (size_t i = 0; i < initialRequests_ && nextIndex_ < urls_.size(); ++i) {
        addHandle(nextIndex_++);
    }

    // 4) запускаем первый вызов, чтобы зарегать timeout = 0
    int stillRunning = 0;
    curl_multi_socket_action(multi_, CURL_SOCKET_TIMEOUT, 0, &stillRunning);

    // 5) основной event loop
    while (stillRunning > 0) {
        int timeout = timer_ms_ >= 0 ? (int)timer_ms_ : 1000;
        epoll_event events[16];
        int n = epoll_wait(epfd_, events, 16, timeout);
        if (n < 0) {
            if (errno == EINTR) continue;
            Logger::warn("Worker: epoll_wait error: %s", std::strerror(errno));
            break;
        }

        if (n == 0) {
            // таймаут по таймеру
            curl_multi_socket_action(multi_, CURL_SOCKET_TIMEOUT, 0, &stillRunning);
        } else {
            for (int i = 0; i < n; ++i) {
                int ev = 0;
                if (events[i].events & EPOLLIN)  ev |= CURL_CSELECT_IN;
                if (events[i].events & EPOLLOUT) ev |= CURL_CSELECT_OUT;
                curl_multi_socket_action(multi_, events[i].data.fd, ev, &stillRunning);
            }
        }

        // 6) вытаскиваем завершённые easy
        while (true) {
            int msgsInQueue = 0;
            CURLMsg* msg = curl_multi_info_read(multi_, &msgsInQueue);
            if (!msg) break;
            if (msg->msg == CURLMSG_DONE) {
                CURL* easy = msg->easy_handle;
                auto itCtx = ctxMap_.find(easy);
                if (itCtx != ctxMap_.end()) {
                    EasyContext* ctx = itCtx->second;
                    curl_easy_getinfo(easy, CURLINFO_RESPONSE_CODE, &ctx->statusCode);

                    // ищем совпадения по ключевым словам
                    bool found = false;
                    for (auto& kw : keywords_) {
                        if (ctx->html.find(kw) != std::string::npos) {
                            found = true;
                            break;
                        }
                    }

                    char* eff_url = nullptr;
                    curl_easy_getinfo(easy, CURLINFO_EFFECTIVE_URL, &eff_url);
                    CURLcode res = msg->data.result;

                    Logger::debug(
                        "DONE: [%ld] %s (CURLcode=%d, found=%s, eff_url=%s)",
                        ctx->statusCode,
                        ctx->url.c_str(),
                        (int)res,
                        (found ? "yes" : "no"),
                        eff_url ? eff_url : "(null)",
                        curl_easy_strerror(res)
                    );

                    // ==== ЗАПИСЬ НА ЛЕТУ ====
                    if (ctx->statusCode == 200 && (keywords_.empty() || found)) {
                        std::string u = ctx->url;
                        if (auto p = u.find("://"); p != std::string::npos)
                            u.erase(0, p + 3);
                        {
                            std::lock_guard<std::mutex> lock(*fileMutex_);
                            std::fprintf(outfp_, "%s\n", u.c_str());
                            std::fflush(outfp_); // Пишем сразу
                        }
                        if (matched_) (*matched_)++;
                    }

                    delete ctx;
                    ctxMap_.erase(itCtx);
                }
                removeHandle(easy);

                // запускаем следующий URL, если он остался
                if (nextIndex_ < urls_.size()) {
                    addHandle(nextIndex_++);
                }
            }
        }
    }

    close(epfd_);
    curl_multi_cleanup(multi_);
}
