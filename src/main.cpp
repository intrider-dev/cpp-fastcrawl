#include "Logger.hpp"
#include "DomainLoader.hpp"
#include "HttpClient.hpp"
#include "CommonTypes.hpp"
#include "CurlShare.hpp"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <algorithm>
#include <mutex>
#include <atomic>
#include <fstream>
#include <sstream>
#include <openssl/sha.h>
#include <thread>
#include <chrono>

static std::vector<std::string> splitCommaList(const std::string& v) {
    std::vector<std::string> result;
    size_t start = 0, p;
    while ((p = v.find(',', start)) != std::string::npos) {
        result.push_back(v.substr(start, p - start));
        start = p + 1;
    }
    if (start < v.size())
        result.push_back(v.substr(start));
    return result;
}

// Добавим разбор аргументов с поддержкой --logfile= и debug
static bool parseArgs(int argc, char* argv[], Args& args, std::string& logfile) {
    for (int i = 1; i < argc; ++i) {
        std::string s(argv[i]);
        if (s == "debug") {
            args.debug = true;
        } else if (s.rfind("--input=", 0) == 0) {
            args.input = s.substr(8);
        } else if (s.rfind("--output=", 0) == 0) {
            args.output = s.substr(9);
        } else if (s.rfind("--threads=", 0) == 0) {
            args.threads = std::atoi(s.c_str() + 10);
        } else if (s.rfind("--contains=", 0) == 0) {
            args.contains = splitCommaList(s.substr(11));
        } else if (s.rfind("--logfile=", 0) == 0) {
            logfile = s.substr(10);
        } else {
            std::fprintf(stderr, "Unknown argument: %s\n", s.c_str());
            return false;
        }
    }
    if (args.input.empty() || args.output.empty() || args.threads <= 0) {
        std::fprintf(stderr, "Required arguments: --input=... --output=... --threads=... [debug] [--contains=...] [--logfile=...]\n");
        return false;
    }
    return true;
}

static std::vector<std::string> buildUrlVariants(const std::string& d) {
    if (d.find("://") != std::string::npos)
        return {d};
    else
        return { "https://" + d, "http://" + d };
}

static std::string hashFilePath(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return "";
    std::ostringstream oss;
    oss << file.rdbuf();
    std::string content = oss.str();

    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(content.data()), content.size(), hash);

    std::ostringstream hashStr;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
        hashStr << std::hex << (int)hash[i];
    }
    return hashStr.str();
}

int main(int argc, char* argv[]) {
    Args args;
    std::string logfile;

    if (!parseArgs(argc, argv, args, logfile)) {
        std::fprintf(stderr,
            "Usage: %s --input=domain_file --output=output_file --threads=N [debug] [--contains=...] [--logfile=...]\n",
            argv[0]);
        return 1;
    }

    curl_global_init(CURL_GLOBAL_ALL);

    initCurlShare();

    Logger::init(args.debug ? Logger::Level::Debug : Logger::Level::Info);

    // 1) вычисляем хеш и ищем state-файл
    std::string hash = hashFilePath(args.input);
    std::string stateFile = "state-" + hash + ".txt";
    std::atomic<size_t> resumeIndex { 0 };
    {
        size_t tmpIndex;
        std::ifstream in(stateFile);
        if (in >> tmpIndex) {
            resumeIndex.store(tmpIndex);
            Logger::info("Resuming from checkpoint %s (index %zu)", stateFile.c_str(), tmpIndex);
        }
    }

    FILE* logfp = nullptr;
    if (!logfile.empty()) {
        logfp = std::fopen(logfile.c_str(), resumeIndex > 0 ? "a" : "w");
        if (!logfp) {
            std::fprintf(stderr, "Cannot open logfile: %s\n", logfile.c_str());
            return 1;
        }
        Logger::setFile(logfp);
    }

    // 2) load domains
    DomainLoader loader(args.input);
    loader.start(); loader.join();
    auto domains = loader.getDomains();
    if (domains.empty()) {
        Logger::error("No domains loaded from %s", args.input.c_str());
        if (logfp) std::fclose(logfp);
        return 1;
    }

    // 3) build URLs
    std::vector<std::string> urls;
    urls.reserve(domains.size()*2);
    for (auto& d : domains) {
        auto v = buildUrlVariants(d);
        urls.insert(urls.end(), v.begin(), v.end());
    }

    // 4) open output file and prepare shared objects
    FILE* outfp = std::fopen(args.output.c_str(), resumeIndex > 0 ? "a" : "w");
    if (!outfp) {
        Logger::error("Cannot open %s", args.output.c_str());
        if (logfp) std::fclose(logfp);
        return 1;
    }
    std::mutex fileMutex;
    std::atomic<size_t> matched{0};

    // 5) run crawl with streaming output
    HttpClient client(
        (size_t)args.threads,
        100,
        outfp,
        &fileMutex,
        &matched,
        &resumeIndex
    );
    client.setResumeIndex(resumeIndex);
    client.setCheckpointFile(stateFile);

    // запускаем фоновую задачу для обновления state на лету
    std::atomic<bool> stopFlag{false};
    std::thread checkpointThread([&]() {
        while (!stopFlag.load()) {
            {
                std::ofstream out(stateFile, std::ios::trunc);
                out << resumeIndex << std::endl;
            }
            std::this_thread::sleep_for(std::chrono::seconds(3));
        }
    });

    client.fetchAll(urls, args.contains);

    stopFlag = true;
    checkpointThread.join();

    std::fclose(outfp);
    Logger::info("Total matched: %zu", matched.load());

    if (logfp) std::fclose(logfp);

    std::call_once(g_shareCleanupOnce, [] {
        if (g_shareHandle) {
            curl_share_cleanup(g_shareHandle);
            g_shareHandle = nullptr;
        }
    });

    curl_global_cleanup();
    return 0;
}
