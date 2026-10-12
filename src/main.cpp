#include <app.hpp>

#include <log.hpp>
#include <spdlog/spdlog.h>

class Handler : public LogHandler {
    void message(MessageSeverity severity, std::string_view message) override {
        switch (severity) {
            case MessageSeverity::INFO:
                spdlog::info(message);
                break;
            case MessageSeverity::WARN:
                spdlog::warn(message);
                break;
            case MessageSeverity::ERROR:
                spdlog::error(message);
                break;
            case MessageSeverity::FATAL:
                spdlog::critical(message);
                break;
            default:
                break;
        }
    }
};

int main() {
    Handler handler;

    App app(handler);

    if (app.init()) {
        app.loadData();
        app.loop();
    }
    app.exit();

    return 0;
}