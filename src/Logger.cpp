#include "Logger.hpp"
#include <cstdio>

Logger::Level Logger::s_level = Logger::Level::Info;
std::mutex    Logger::s_mutex;

void Logger::init(Level lvl) {
    s_level = lvl;
}

bool Logger::isDebug() {
    return s_level == Level::Debug;
}

void Logger::error(const char* fmt, ...) {
    if (s_level < Level::Error) return;
    va_list ap; va_start(ap, fmt);
    log(Level::Error, fmt, ap);
    va_end(ap);
}

void Logger::warn(const char* fmt, ...) {
    if (s_level < Level::Warn) return;
    va_list ap; va_start(ap, fmt);
    log(Level::Warn, fmt, ap);
    va_end(ap);
}

void Logger::info(const char* fmt, ...) {
    if (s_level < Level::Info) return;
    va_list ap; va_start(ap, fmt);
    log(Level::Info, fmt, ap);
    va_end(ap);
}

void Logger::debug(const char* fmt, ...) {
    if (s_level < Level::Debug) return;
    va_list ap; va_start(ap, fmt);
    log(Level::Debug, fmt, ap);
    va_end(ap);
}

void Logger::log(Level lvl, const char* fmt, va_list ap) {
    static const char* names[] = { "ERROR","WARN","INFO","DEBUG" };
    std::lock_guard<std::mutex> lk(s_mutex);
    std::fprintf(stderr, "[%s] ", names[int(lvl)]);
    std::vfprintf(stderr, fmt, ap);
    std::fprintf(stderr, "\n");
}
