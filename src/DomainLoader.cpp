// src/DomainLoader.cpp
#include "DomainLoader.hpp"
#include "Worker.hpp"
#include "Logger.hpp"
#include <fstream>
#include <string>

DomainLoader::DomainLoader(const std::string& fn)
    : filename(fn)
{}

DomainLoader::~DomainLoader() {}

void DomainLoader::start() {
    loaderThread = std::thread(&DomainLoader::run, this);
}

void DomainLoader::join() {
    if (loaderThread.joinable())
        loaderThread.join();
}

void DomainLoader::run() {
    std::ifstream in(filename);
    if (!in) {
        Logger::error("Failed to open domain file: %s", filename.c_str());
        Worker::notifyFinished();
        return;
    }
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty())
            Worker::enqueueDomain(line);
    }
    Worker::notifyFinished();
}
