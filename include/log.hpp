#pragma once

#include <cstdint>
#include <string_view>
#include <format>

enum class MessageSeverity : uint8_t { INFO, WARN, ERROR, FATAL };

class LogHandler {
public:
    LogHandler() = default;

    virtual void message(MessageSeverity severity, std::string_view message) = 0;
};

class Log {
public:
    explicit Log(LogHandler* handler) noexcept
        : handler(handler) {}

    template <typename... Args>
    void log(MessageSeverity severity, std::format_string<Args...> fmt_str, Args&&... args) const {
        if (handler) {
            std::string formatted = std::format(fmt_str, std::forward<Args>(args)...);
            handler->message(severity, formatted);
        }
    }

    template <typename... Args>
    void info(std::format_string<Args...> fmt_str, Args&&... args) const {
        log(MessageSeverity::INFO, fmt_str, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void warn(std::format_string<Args...> fmt_str, Args&&... args) const {
        log(MessageSeverity::WARN, fmt_str, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void error(std::format_string<Args...> fmt_str, Args&&... args) const {
        log(MessageSeverity::ERROR, fmt_str, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void fatal(std::format_string<Args...> fmt_str, Args&&... args) const {
        log(MessageSeverity::FATAL, fmt_str, std::forward<Args>(args)...);
    }

private:
    LogHandler* handler = nullptr;
};