#pragma once
#include <string>
#include <vector>
#include <thread>

class DomainLoader {
public:
    explicit DomainLoader(const std::string& filename);
    void start();
    void join();
    std::vector<std::string> getDomains() const;

private:
    std::string filename_;
    std::vector<std::string> domains_;
    std::thread loaderThread_;
    void loaderFunc();
};
