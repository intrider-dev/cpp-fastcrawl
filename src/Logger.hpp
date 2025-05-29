// Logger.hpp
#pragma once
#include <mutex>
#include <cstdarg>
#include <cstdio>

class Logger {
public:
    enum class Level { Error = 0, Warn = 1, Info = 2, Debug = 3 };

    static void init(Level lvl);
    static void setFile(FILE* fp);
    static bool isDebug();

    static void error(const char* fmt, ...);
    static void warn(const char* fmt, ...);
    static void info(const char* fmt, ...);
    static void debug(const char* fmt, ...);

private:
    static void log(Level lvl, const char* fmt, va_list ap);

    static Level s_level;
    static std::mutex s_mutex;
    static FILE* s_fp;
};
