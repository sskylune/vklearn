#pragma once

#include <cstdint>
#include <string_view>

enum class MessageSeverity : uint8_t { INFO, WARN, ERROR, FATAL };

// Подумать над этим
class LogHandler {
  public:
    LogHandler() = default;

    virtual void message(MessageSeverity severity, std::string_view message) = 0;
};

class Log {
  public:
    explicit Log(LogHandler* handler) noexcept : handler(handler) {}

    void log(MessageSeverity severity, std::string_view message) const {
        if (handler)
            handler->message(severity, message);
    }

    void info(std::string_view message) const {
        log(MessageSeverity::INFO, message);
    }

    void warn(std::string_view message) const {
        log(MessageSeverity::WARN, message);
    }

    void error(std::string_view message) const {
        log(MessageSeverity::ERROR, message);
    }

    void fatal(std::string_view message) const {
        log(MessageSeverity::FATAL, message);
    }

  private:
    LogHandler* handler = nullptr;
};