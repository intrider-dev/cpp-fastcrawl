// src/Worker.hpp
#ifndef WORKER_HPP
#define WORKER_HPP

#include <string>
#include <queue>
#include <mutex>
#include <thread>
#include <vector>
#include <condition_variable>

class Worker {
public:
    Worker() = default;
    void operator()();

    static void startThreads(int count, FILE* outfp);
    static void enqueueDomain(const std::string& domain);
    static void notifyFinished();
    static void joinThreads();
    static void setMatchWords(const std::vector<std::string>& words);

    static std::mutex outputMutex;

private:
    static std::queue<std::string> domainQueue;
    static bool loadingDone;
    static std::mutex queueMutex;
    static std::condition_variable condVar;
    static std::vector<std::thread> threads;
    static FILE* outputFile;
    static std::vector<std::string> matchWords;
};

#endif // WORKER_HPP
