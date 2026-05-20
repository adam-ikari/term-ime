#include "core/event_loop.hpp"
#include "core/app.hpp"
#include "core/config.hpp"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <unistd.h>
#include <signal.h>
#include <cstdio>
#include <cstdlib>
#include <filesystem>

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    // 设置日志输出到文件
    try {
        std::string log_dir = std::filesystem::path(getenv("HOME") ? getenv("HOME") : "/tmp") / ".cache" / "term-ime";
        std::filesystem::create_directories(log_dir);
        auto logger = spdlog::basic_logger_mt("term-ime", log_dir + "/term-ime.log", true);
        spdlog::set_default_logger(logger);
        spdlog::set_level(spdlog::level::debug);
    } catch (const std::exception& e) {
        // 如果无法创建文件日志，禁用日志
        spdlog::set_level(spdlog::level::off);
    }

    spdlog::info("term-ime starting");

    // Load configuration
    std::string config_path = AppConfig::default_path();
    if (argc > 1) {
        config_path = argv[1];
    }
    AppConfig config = AppConfig::load(config_path);
    spdlog::info("Loaded config from: {}", config_path);

    // Override shell from environment if not set in config
    if (config.shell.empty() || config.shell == "/bin/bash") {
        const char* shell_env = getenv("SHELL");
        if (shell_env) {
            config.shell = shell_env;
        }
    }

    // Create event loop
    spdlog::info("Creating event loop");
    EventLoop loop;

    // Create application
    spdlog::info("Creating app");
    App app;
    if (!app.init(config)) {
        spdlog::error("App init failed");
        return 1;
    }

    spdlog::info("Registering callbacks");

    // Register PTY reader
    loop.watch_fd(app.pty_fd(), [&app, &loop](const char* data, size_t len) {
        if (len == 0 || data == nullptr) {
            // PTY closed (EOF), exit gracefully
            spdlog::info("PTY closed, exiting");
            app.on_quit(0);
            loop.stop();
        } else {
            app.on_pty_data(data, len);
        }
    });

    // Register keyboard reader
    loop.watch_fd(STDIN_FILENO, [&app](const char* data, size_t len) {
        app.on_keyboard_data(data, len);
    });

    // Register signal handlers
    loop.watch_signal(SIGWINCH, [&app](int signum) {
        app.on_resize(signum);
    });

    loop.watch_signal(SIGINT, [&app, &loop](int signum) {
        app.on_quit(signum);
        loop.stop();
    });

    loop.watch_signal(SIGTERM, [&app, &loop](int signum) {
        app.on_quit(signum);
        loop.stop();
    });

    // Run event loop
    spdlog::info("Starting event loop");
    loop.run();
    spdlog::info("Event loop finished");

    return 0;
}
