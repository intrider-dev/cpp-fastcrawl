#include "Logger.hpp"
#include "DomainLoader.hpp"
#include "HttpClient.hpp"
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <mutex>
#include <algorithm>

static std::vector<std::string> parseContainsArgs(int argc, char* argv[]) {
    std::vector<std::string> result;
    const std::string prefix = "--contains=";
    for (int i = 4; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg.rfind(prefix, 0) == 0) {
            std::string value = arg.substr(prefix.size());
            size_t pos = 0;
            while ((pos = value.find(',')) != std::string::npos) {
                result.emplace_back(value.substr(0, pos));
                value.erase(0, pos + 1);
            }
            if (!value.empty())
                result.emplace_back(std::move(value));
        }
    }
    return result;
}

std::vector<std::string> buildUrlVariants(const std::string& domain) {
    std::vector<std::string> urls;
    if (domain.find("://") != std::string::npos) {
        urls.push_back(domain);
    } else {
        urls.push_back("https://" + domain);
        urls.push_back("http://" + domain);
    }
    return urls;
}

int main(int argc, char* argv[]) {
    if (argc < 4) {
        Logger::error("Usage: %s <domain_file> <output_file> <threads> [debug] [--contains=word1,word2,...]", argv[0]);
        return 1;
    }

    const std::string domainFile = argv[1];
    const std::string outFile    = argv[2];
    int numThreads               = std::atoi(argv[3]);
    if (numThreads <= 0) {
        Logger::error("Invalid thread count: %d", numThreads);
        return 1;
    }

    // Уровень логирования
    Logger::Level lvl = Logger::Level::Info;
    for (int i = 4; i < argc; ++i) {
        if (std::string(argv[i]) == "debug") {
            lvl = Logger::Level::Debug;
            break;
        }
    }
    Logger::init(lvl);

    // Ключевые слова для поиска
    std::vector<std::string> matchWords = parseContainsArgs(argc, argv);

    // 1. Загружаем домены из файла
    DomainLoader loader(domainFile);
    loader.start();
    loader.join();
    std::vector<std::string> domains = loader.getDomains();
    if (domains.empty()) {
        Logger::error("No domains loaded from: %s", domainFile.c_str());
        return 1;
    }

    // 2. Формируем url-варианты (http/https) для каждого домена
    std::vector<std::string> urls;
    urls.reserve(domains.size() * 2);
    for (const std::string& domain : domains) {
        auto variants = buildUrlVariants(domain);
        urls.insert(urls.end(), variants.begin(), variants.end());
    }

    // 3. Открываем файл для потоковой записи
    FILE* outfp = std::fopen(outFile.c_str(), "w");
    if (!outfp) {
        Logger::error("Failed to open output file: %s", outFile.c_str());
        return 1;
    }
    std::mutex fileMutex;

    // 4. Запускаем краулер с потоковой записью
    HttpClient client(static_cast<size_t>(numThreads), 100);
    auto results = client.fetchAll(urls, matchWords, outfp, &fileMutex);

    // 5. После завершения всех worker'ов — файл можно закрывать
    std::fclose(outfp);

    // 6. (Опционально) статистика: подсчёт совпавших доменов
    size_t matched = 0;
    for (const auto& resp : results) {
        if (resp.statusCode == 200 && (matchWords.empty() || resp.keywordFound))
            ++matched;
    }
    Logger::info("Total matched: %zu", matched);

    return 0;
}
