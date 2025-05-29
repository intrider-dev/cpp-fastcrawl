#include "DomainLoader.hpp"
#include <fstream>
#include <algorithm>
#include <cctype>

DomainLoader::DomainLoader(const std::string& filename)
    : filename_(filename)
{}

void DomainLoader::start() {
    loaderThread_ = std::thread(&DomainLoader::loaderFunc, this);
}

void DomainLoader::join() {
    if (loaderThread_.joinable())
        loaderThread_.join();
}

std::vector<std::string> DomainLoader::getDomains() const {
    return domains_;
}

void DomainLoader::loaderFunc() {
    std::ifstream infile(filename_);
    std::string line;
    while (std::getline(infile, line)) {
        // Обрезаем пробелы и спецсимволы
        line.erase(std::remove_if(line.begin(), line.end(), [](unsigned char c) {
            return std::isspace(c) || c == '\r' || c == '\n';
        }), line.end());
        if (!line.empty()) {
            domains_.push_back(line);
        }
    }
}
