// src/Logger.cpp
#include "Logger.hpp"

Logger::Level Logger::currentLevel = Logger::Level::Info;
std::mutex    Logger::mtx;

void Logger::init(Level level) {
    currentLevel = level;
}

void Logger::log(Level msgLevel, const char* fmt, va_list args) {
    if (msgLevel <= currentLevel) {
        std::lock_guard<std::mutex> lock(mtx);
        const char* lvl = (msgLevel == Level::Error ? "ERROR" :
                           msgLevel == Level::Info  ? "INFO"  :
                                                      "DEBUG");
        std::time_t t = std::time(nullptr);
        char tb[20];
        std::strftime(tb, sizeof(tb), "%Y-%m-%d %H:%M:%S", std::localtime(&t));
        std::fprintf(stderr, "%s [%s] ", tb, lvl);
        std::vfprintf(stderr, fmt, args);
        std::fprintf(stderr, "\n");
    }
}

void Logger::error(const char* fmt, ...) {
    va_list ap; va_start(ap, fmt);
    log(Level::Error, fmt, ap);
    va_end(ap);
}

void Logger::info(const char* fmt, ...) {
    va_list ap; va_start(ap, fmt);
    log(Level::Info, fmt, ap);
    va_end(ap);
}

void Logger::debug(const char* fmt, ...) {
    va_list ap; va_start(ap, fmt);
    log(Level::Debug, fmt, ap);
    va_end(ap);
}
