// src/main.cpp
#include "Logger.hpp"
#include "DomainLoader.hpp"
#include "Worker.hpp"
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <algorithm>

static std::vector<std::string> parseContainsArgs(int argc, char* argv[]) {
    std::vector<std::string> result;
    for (int i = 4; i < argc; ++i) {
        std::string arg = argv[i];
        const std::string prefix = "--contains=";
        if (arg.rfind(prefix, 0) == 0) {
            std::string value = arg.substr(prefix.size());
            size_t pos = 0;
            while ((pos = value.find(',')) != std::string::npos) {
                result.push_back(value.substr(0, pos));
                value.erase(0, pos + 1);
            }
            if (!value.empty())
                result.push_back(value);
        }
    }
    return result;
}

int main(int argc, char* argv[]) {
    if (argc < 4) {
        Logger::error("Usage: %s <domain_file> <output_file> <threads> [debug] [--contains=word1,word2,...]", argv[0]);
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
    for (int i = 4; i < argc; ++i) {
        if (std::string(argv[i]) == "debug") {
            lvl = Logger::Level::Debug;
            break;
        }
    }

    Logger::init(lvl);
    std::vector<std::string> matchWords = parseContainsArgs(argc, argv);
    Worker::setMatchWords(matchWords);

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
