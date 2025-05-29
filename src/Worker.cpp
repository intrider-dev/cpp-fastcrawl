#include "Worker.hpp"
#include "Logger.hpp"
#include "CurlShare.hpp"

#include <sys/epoll.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <mutex>
#include <map>
#include <sstream>
#include <cassert>
#include <unordered_set>

// Конструктор
Worker::Worker(const std::vector<std::string>& urls,
               std::atomic<size_t>* nextIndex,
               const std::vector<std::string>& keywords,
               size_t initialRequests,
               FILE* outfp,
               std::mutex* fileMutex,
               std::atomic<size_t>* matched)
    : urls_(urls),
      nextIndex_(nextIndex),  // указатель
      keywords_(keywords),
      initialRequests_(initialRequests),
      multi_(nullptr),
      epfd_(-1),
      timer_ms_(-1),
      outfp_(outfp),
      fileMutex_(fileMutex),
      matched_(matched)
{}

// Callback для записи тела ответа
size_t Worker::writeCallback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* ctx = static_cast<EasyContext*>(userdata);
    ctx->html.append(ptr, size * nmemb);
    return size * nmemb;
}

// Callback для интеграции с epoll
int Worker::curlSocketCallback(CURL* /*easy*/, curl_socket_t s, int what, void* userp, void* /*sockp*/) {
    auto* self = static_cast<Worker*>(userp);
    epoll_event ev{};
    ev.data.fd = s;

    uint32_t events = 0;
    switch (what) {
        case CURL_POLL_IN:    events = EPOLLIN;             break;
        case CURL_POLL_OUT:   events = EPOLLOUT;            break;
        case CURL_POLL_INOUT: events = EPOLLIN | EPOLLOUT;  break;
        case CURL_POLL_REMOVE:
            epoll_ctl(self->epfd_, EPOLL_CTL_DEL, s, nullptr);
            self->sockFlags_.erase(s);
            return 0;
        default:
            return 0;
    }
    ev.events = events | EPOLLET;

    auto it = self->sockFlags_.find(s);
    if (it == self->sockFlags_.end()) {
        if (epoll_ctl(self->epfd_, EPOLL_CTL_ADD, s, &ev) == -1) {
            Logger::warn("Worker: epoll_ctl ADD failed: %s", strerror(errno));
        } else {
            self->sockFlags_[s] = events;
        }
    } else {
        if (epoll_ctl(self->epfd_, EPOLL_CTL_MOD, s, &ev) == -1) {
            Logger::warn("Worker: epoll_ctl MOD failed: %s", strerror(errno));
        } else {
            self->sockFlags_[s] = events;
        }
    }

    curl_multi_assign(self->multi_, s, self);
    return 0;
}

// Callback для таймера
int Worker::timerCb(CURLM* /*multi*/, long timeout_ms, void* userp) {
    auto* self = static_cast<Worker*>(userp);
    self->timer_ms_ = timeout_ms;
    return 0;
}

// Добавить новый easy-handle
void Worker::addHandle(size_t idx) {
    if (idx >= urls_.size()) {
        Logger::warn("Worker: out-of-bounds index: %zu", idx);
        return;
    }

    const auto& url = urls_[idx];
    CURL* easy = curl_easy_init();
    if (!easy) {
        Logger::warn("Worker: curl_easy_init failed for %s", url.c_str());
        return;
    }
    auto* ctx = new EasyContext{url, "", 0L};
    ctxMap_[easy] = ctx;

    curl_easy_setopt(easy, CURLOPT_URL,            ctx->url.c_str());
    curl_easy_setopt(easy, CURLOPT_WRITEFUNCTION,  writeCallback);
    curl_easy_setopt(easy, CURLOPT_WRITEDATA,      ctx);
    curl_easy_setopt(easy, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(easy, CURLOPT_TIMEOUT,        60L);
    curl_easy_setopt(easy, CURLOPT_CONNECTTIMEOUT, 15L);
    curl_easy_setopt(easy, CURLOPT_NOSIGNAL,       1L);
    if (g_shareHandle)
        curl_easy_setopt(easy, CURLOPT_SHARE, g_shareHandle);

    curl_multi_add_handle(multi_, easy);
}

void Worker::setResumeIndexPtr(std::atomic<size_t>* ptr) {
    resumeIndex_ = ptr;
}

// Безопасно удалить easy-handle
void Worker::safeRemove(CURL* easy) {
    if (!easy || removedHandles_.count(easy)) return;

    removedHandles_.insert(easy);
    curl_multi_remove_handle(multi_, easy);

    auto it = ctxMap_.find(easy);
    if (it != ctxMap_.end()) {
        delete it->second;
        ctxMap_.erase(it);
    }

    curl_easy_cleanup(easy);
}

// Главный оператор потока
void Worker::operator()() {
    initCurlShare();

    if (Logger::isDebug()) {
        std::ostringstream oss;
        oss << "Worker: matchWords = [";
        for (size_t i = 0; i < keywords_.size(); ++i) {
            if (i) oss << ", ";
            oss << keywords_[i];
        }
        oss << "]";
        Logger::debug("%s", oss.str().c_str());
    }

    multi_ = curl_multi_init();
    if (!multi_) {
        Logger::warn("Worker: curl_multi_init failed");
        return;
    }

    curl_multi_setopt(multi_, CURLMOPT_PIPELINING, CURLPIPE_MULTIPLEX);
    curl_multi_setopt(multi_, CURLMOPT_SOCKETFUNCTION, curlSocketCallback);
    curl_multi_setopt(multi_, CURLMOPT_SOCKETDATA, this);
    curl_multi_setopt(multi_, CURLMOPT_TIMERFUNCTION, timerCb);
    curl_multi_setopt(multi_, CURLMOPT_TIMERDATA, this);

    epfd_ = epoll_create1(0);
    if (epfd_ < 0) {
        Logger::warn("Worker: epoll_create1 failed: %s", strerror(errno));
        int stillRunning = 0;
        curl_multi_socket_action(multi_, CURL_SOCKET_TIMEOUT, 0, &stillRunning);
        curl_multi_cleanup(multi_);
        return;
    }

    for (size_t i = 0; i < initialRequests_ && nextIndex_->load() < urls_.size(); ++i) {
        size_t idx = nextIndex_->fetch_add(1);
        addHandle(idx);
        if (resumeIndex_) resumeIndex_->store(idx);
    }

    int stillRunning = 0;
    curl_multi_socket_action(multi_, CURL_SOCKET_TIMEOUT, 0, &stillRunning);

    while (stillRunning > 0) {
        int timeout = timer_ms_ >= 0 ? int(timer_ms_) : 1000;
        epoll_event events[16];
        int n = epoll_wait(epfd_, events, 16, timeout);
        if (n < 0) {
            if (errno == EINTR) continue;
            Logger::warn("Worker: epoll_wait error: %s", strerror(errno));
            break;
        }

        if (n == 0) {
            curl_multi_socket_action(multi_, CURL_SOCKET_TIMEOUT, 0, &stillRunning);
        } else {
            for (int i = 0; i < n; ++i) {
                int ev = 0;
                if (events[i].events & EPOLLIN)  ev |= CURL_CSELECT_IN;
                if (events[i].events & EPOLLOUT) ev |= CURL_CSELECT_OUT;
                curl_multi_socket_action(multi_, events[i].data.fd, ev, &stillRunning);
            }
        }

        while (true) {
            int msgs;
            CURLMsg* msg = curl_multi_info_read(multi_, &msgs);
            if (!msg) break;
            if (msg->msg != CURLMSG_DONE) continue;

            CURL* easy = msg->easy_handle;
            auto it = ctxMap_.find(easy);
            if (it == ctxMap_.end()) {
                safeRemove(easy);
                continue;
            }

            auto* ctx = it->second;
            curl_easy_getinfo(easy, CURLINFO_RESPONSE_CODE, &ctx->statusCode);

            bool allFound = true;
            for (auto& kw : keywords_) {
                if (ctx->html.find(kw) == std::string::npos) {
                    allFound = false;
                    break;
                }
            }

            char* eff = nullptr;
            curl_easy_getinfo(easy, CURLINFO_EFFECTIVE_URL, &eff);
            Logger::debug(
                "DONE: [%ld] %s (CURLcode=%d, allFound=%s, eff_url=%s)",
                ctx->statusCode,
                ctx->url.c_str(),
                (int)msg->data.result,
                allFound ? "yes" : "no",
                eff ? eff : "(null)"
            );

            if (ctx->statusCode == 200 && (keywords_.empty() || allFound)) {
                std::string u = ctx->url;
                if (auto p = u.find("://"); p != std::string::npos)
                    u.erase(0, p + 3);
                {
                    std::lock_guard<std::mutex> lock(*fileMutex_);
                    std::fprintf(outfp_, "%s\n", u.c_str());
                    std::fflush(outfp_);
                }
                if (matched_) (*matched_)++;
            }

            safeRemove(easy);

            if (nextIndex_->load() < urls_.size()) {
                size_t idx = nextIndex_->fetch_add(1);
                addHandle(idx);
                if (resumeIndex_) resumeIndex_->store(idx);
            }
        }
    }

    {
        int msgs_left = 0;
        curl_multi_socket_action(multi_, CURL_SOCKET_TIMEOUT, 0, &stillRunning);
        while (CURLMsg* msg = curl_multi_info_read(multi_, &msgs_left)) {
            if (msg->msg == CURLMSG_DONE) {
                safeRemove(msg->easy_handle);
            }
        }
    }

    for (auto& kv : ctxMap_) {
        CURL* easy = kv.first;
        EasyContext* ctx = kv.second;
        if (!removedHandles_.count(easy)) {
            curl_multi_remove_handle(multi_, easy);
            curl_easy_cleanup(easy);
        }
        delete ctx;
    }
    ctxMap_.clear();

    if (multi_) {
        curl_multi_cleanup(multi_);
        multi_ = nullptr;
    }

    if (epfd_ >= 0) {
        close(epfd_);
        epfd_ = -1;
    }
}