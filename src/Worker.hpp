// src/Worker.hpp
#ifndef WORKER_HPP
#define WORKER_HPP

#include <queue>
#include <vector>
#include <string>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <cstdio>

class Worker {
public:
    static void startThreads(int count, FILE* outfp);
    static void enqueueDomain(const std::string& domain);
    static void notifyFinished();
    static void joinThreads();
    void operator()();

    static std::mutex outputMutex;  // for HttpClient to lock when writing

private:
    static std::queue<std::string>    domainQueue;
    static bool                       loadingDone;
    static std::mutex                 queueMutex;
    static std::condition_variable    condVar;
    static std::vector<std::thread>   threads;
    static FILE*                      outputFile;
};

#endif // WORKER_HPP
