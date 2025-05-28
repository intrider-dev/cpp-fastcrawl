// src/DomainLoader.hpp
#ifndef DOMAINLOADER_HPP
#define DOMAINLOADER_HPP

#include <string>
#include <thread>

class DomainLoader {
public:
    DomainLoader(const std::string& filename);
    ~DomainLoader();

    void start();
    void join();

private:
    void run();
    std::thread loaderThread;
    std::string filename;
};

#endif // DOMAINLOADER_HPP
