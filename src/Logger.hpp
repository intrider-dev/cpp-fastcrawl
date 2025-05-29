#pragma once
#include <mutex>
#include <cstdarg>

class Logger {
public:
    enum class Level { Error=0, Warn, Info, Debug };

    // Инициализируем единожды в main()
    static void init(Level lvl);

    // Проверка уровня
    static bool isDebug();

    // Основные методы
    static void error(const char* fmt, ...);
    static void warn (const char* fmt, ...);
    static void info (const char* fmt, ...);
    static void debug(const char* fmt, ...);

private:
    static void log(Level lvl, const char* fmt, va_list ap);

    static Level      s_level;
    static std::mutex s_mutex;
};
