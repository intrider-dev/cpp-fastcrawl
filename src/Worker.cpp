// src/Worker.cpp
#include "Worker.hpp"
#include "HttpClient.hpp"
#include "Logger.hpp"

std::queue<std::string>  Worker::domainQueue;
bool                     Worker::loadingDone = false;
std::mutex               Worker::queueMutex;
std::condition_variable  Worker::condVar;
std::vector<std::thread> Worker::threads;
FILE*                    Worker::outputFile = nullptr;
std::mutex               Worker::outputMutex;

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

void Worker::operator()() {
    HttpClient client;
    std::string domain;
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
        bool ok = client.fetchAndWrite(domain, outputFile);
        if (!ok)
            Logger::error("Worker: failed fetching %s", domain.c_str());
        else
            Logger::debug("Worker: fetched %s", domain.c_str());
    }
}
