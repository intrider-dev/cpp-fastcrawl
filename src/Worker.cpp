// src/Worker.cpp
#include "Worker.hpp"
#include "HttpClient.hpp"
#include "Logger.hpp"
#include <algorithm>

std::queue<std::string>  Worker::domainQueue;
bool                     Worker::loadingDone = false;
std::mutex               Worker::queueMutex;
std::condition_variable  Worker::condVar;
std::vector<std::thread> Worker::threads;
FILE*                    Worker::outputFile = nullptr;
std::mutex               Worker::outputMutex;
std::vector<std::string> Worker::matchWords;

void Worker::startThreads(int count, FILE* outfp) {
    outputFile = outfp;
    for (int i = 0; i < count; ++i) {
        threads.emplace_back(Worker());
    }
}

void Worker::enqueueDomain(const std::string& domain) {
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        domainQueue.push(domain);
    }
    condVar.notify_one();
}

void Worker::notifyFinished() {
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        loadingDone = true;
    }
    condVar.notify_all();
}

void Worker::joinThreads() {
    for (auto& t : threads) {
        if (t.joinable())
            t.join();
    }
}

void Worker::setMatchWords(const std::vector<std::string>& words) {
    matchWords = words;
}

void Worker::operator()() {
    HttpClient client;
    std::string domain;

    static std::once_flag onceFlag;
    std::call_once(onceFlag, []() {
        Logger::debug("Worker: searching for words:");
        for (const auto& w : matchWords) {
            Logger::debug("  - %s", w.c_str());
        }
    });

    while (true) {
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            condVar.wait(lock, [] {
                return !domainQueue.empty() || loadingDone;
            });
            if (domainQueue.empty() && loadingDone)
                break;
            domain = std::move(domainQueue.front());
            domainQueue.pop();
        }

        domain.erase(std::remove(domain.begin(), domain.end(), '\r'), domain.end());
        domain.erase(std::remove(domain.begin(), domain.end(), '\n'), domain.end());
        std::string full_url = domain; // "https://" + domain;

        std::string body;
        bool ok = client.fetch(full_url, body);
        Logger::debug("Body length for %s (--> %zu <--)", domain.c_str(), body.size());
        if (!ok) {
            Logger::error("Worker: failed fetching %s", domain.c_str());
        } else {
            Logger::debug("Worker: fetched and saved %s", domain.c_str());
        }

        std::string body_lower = body;
        std::transform(body_lower.begin(), body_lower.end(), body_lower.begin(), ::tolower);

        bool matched = true;  // предполагаем, что все слова есть
        for (const auto& word : matchWords) {
            std::string word_lower = word;
            std::transform(word_lower.begin(), word_lower.end(), word_lower.begin(), ::tolower);

            if (body_lower.find(word_lower) == std::string::npos) {
                matched = false;  // если хоть одно слово не найдено, ставим false
                break;
            }
        }

        if (matched) {
            std::lock_guard<std::mutex> lock(outputMutex);
            std::fprintf(outputFile, "%s\n", domain.c_str());
            std::fflush(outputFile);
            Logger::debug("Worker: matched all words in domain %s", domain.c_str());
        } else {
            Logger::debug("Worker: not matched %s", domain.c_str());
        }
    }
}
