// src/Logger.hpp
#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <string>
#include <mutex>
#include <cstdarg>
#include <cstdio>
#include <ctime>

class Logger {
public:
    enum class Level { Error = 0, Info = 1, Debug = 2 };

    static void init(Level level = Level::Info);
    static void error(const char* fmt, ...);
    static void info (const char* fmt, ...);
    static void debug(const char* fmt, ...);

private:
    static Level    currentLevel;
    static std::mutex mtx;
    static void     log(Level msgLevel, const char* fmt, va_list args);
};

#endif // LOGGER_HPP
