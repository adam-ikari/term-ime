#include "core/event_loop.hpp"
#include "core/app.hpp"
#include "core/config.hpp"
#include "util/i18n.hpp"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <unistd.h>
#include <signal.h>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iostream>

// Log level named in the config; unknown names keep "warn" rather than turning
// logging off (spdlog::level::from_str maps them to "off", which would hide the
// very errors a misconfigured build produces).
static spdlog::level::level_enum config_log_level(const std::string& name) {
    if (name == "trace") return spdlog::level::trace;
    if (name == "debug") return spdlog::level::debug;
    if (name == "info") return spdlog::level::info;
    if (name == "error") return spdlog::level::err;
    if (name == "warn") return spdlog::level::warn;
    spdlog::warn("config log_level \"{}\" is not one of trace/debug/info/warn/error, using warn", name);
    return spdlog::level::warn;
}

int main(int argc, char* argv[]) {
    // 设置日志输出到文件
    std::string log_file;  // path the running logger writes to (shown on failure)
    bool file_logging = false;
    try {
        const std::filesystem::path log_dir =
            std::filesystem::path(getenv("HOME") ? getenv("HOME") : "/tmp") / ".cache" / "term-ime";
        std::filesystem::create_directories(log_dir);
        log_file = (log_dir / "term-ime.log").string();
        auto logger = spdlog::basic_logger_mt("term-ime", log_file, true);
        spdlog::set_default_logger(logger);
        spdlog::set_level(spdlog::level::debug);
        file_logging = true;
    } catch (const std::exception& e) {
        // 如果无法创建文件日志，禁用日志。The default logger writes to stderr,
        // which would scribble over the alternate screen, so nothing is logged.
        log_file.clear();
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

    // The boot logger above exists only so that loading the config could be
    // logged; from here the config owns the level and the file.
    if (file_logging) {
        spdlog::set_level(config_log_level(config.log_level));
        if (!config.log_file.empty() && config.log_file != log_file) {
            try {
                std::filesystem::create_directories(std::filesystem::path(config.log_file).parent_path());
                spdlog::drop("term-ime");
                spdlog::set_default_logger(spdlog::basic_logger_mt("term-ime", config.log_file, true));
                log_file = config.log_file;
            } catch (const std::exception& e) {
                spdlog::error("Cannot log to {}: {}", config.log_file, e.what());
                spdlog::set_level(spdlog::level::off);
            }
        }
    }

    // Initialize i18n based on active language
    I18n::Lang i18n_lang = I18n::Lang::EN;
    if (config.active_language == "zh-Hans") {
        i18n_lang = I18n::Lang::ZH_CN;
    }
    I18n::init(i18n_lang);

    // Create event loop
    spdlog::info("Creating event loop");
    EventLoop loop;

    // Create application
    spdlog::info("Creating app");
    App app;
    if (!app.init(config, &loop)) {
        spdlog::error("App init failed");
        // Print error to stderr so user can see it even when not in TTY
        std::cerr << std::endl;
        std::cerr << "╔══════════════════════════════════════════════════════════════╗" << std::endl;
        std::cerr << "║  term-ime 启动失败                                           ║" << std::endl;
        std::cerr << "╠══════════════════════════════════════════════════════════════╣" << std::endl;
        std::cerr << "║  原因: 初始化失败                                            ║" << std::endl;
        std::cerr << "║                                                              ║" << std::endl;
        std::cerr << "║  可能的原因:                                                ║" << std::endl;
        std::cerr << "║    • 未连接到终端 (stdin/stdout 被重定向)                   ║" << std::endl;
        std::cerr << "║    • 终端不支持 alternate screen (需要 xterm-256color 等)   ║" << std::endl;
        std::cerr << "║    • 无法创建 PTY (权限不足)                                ║" << std::endl;
        std::cerr << "║                                                              ║" << std::endl;
        std::cerr << "║  详细日志: " << (log_file.empty() ? std::string("(文件日志不可用)") : log_file) << std::endl;
        std::cerr << "╚══════════════════════════════════════════════════════════════╝" << std::endl;
        std::cerr << std::endl;
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
    loop.watch_fd(STDIN_FILENO, [&app, &loop](const char* data, size_t len) {
        if (len == 0 || data == nullptr) {
            // stdin reached EOF/error (terminal gone); the EventLoop already
            // dropped the watch, so exit gracefully instead of spinning.
            spdlog::info("Keyboard input closed, exiting");
            app.on_quit(0);
            loop.stop();
            return;
        }
        app.on_keyboard_data(data, len);
        if (app.quit_requested()) {
            loop.stop();
        }
    });

    // Register signal handlers
    loop.watch_signal(SIGWINCH, [&app](int signum) { app.on_resize(signum); });

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
