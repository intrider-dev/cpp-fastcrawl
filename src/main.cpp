// src/main.cpp
#include "Logger.hpp"
#include "DomainLoader.hpp"
#include "Worker.hpp"
#include <cstdio>
#include <cstdlib>
#include <string>

int main(int argc, char* argv[]) {
    if (argc < 4) {
        Logger::error("Usage: %s <domain_file> <output_file> <threads> [debug]", argv[0]);
        return 1;
    }
    std::string domainFile = argv[1];
    std::string outFile    = argv[2];
    int numThreads         = std::atoi(argv[3]);
    if (numThreads <= 0) {
        Logger::error("Invalid thread count: %d", numThreads);
        return 1;
    }

    Logger::Level lvl = Logger::Level::Info;
    if (argc >= 5 && std::string(argv[4]) == "debug")
        lvl = Logger::Level::Debug;
    Logger::init(lvl);

    FILE* outfp = std::fopen(outFile.c_str(), "w");
    if (!outfp) {
        Logger::error("Failed to open output file: %s", outFile.c_str());
        return 1;
    }

    DomainLoader loader(domainFile);
    loader.start();
    Worker::startThreads(numThreads, outfp);

    loader.join();
    Worker::notifyFinished();
    Worker::joinThreads();

    std::fclose(outfp);
    return 0;
}
