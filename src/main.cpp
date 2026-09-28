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
#include <chrono>
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

// A file logger that only flushes when the process exits cleanly loses exactly
// the lines that would explain an abrupt death (observed: a multiplexer killing
// the pane left a log that stopped mid-boot). warn and above go out at once;
// everything else waits at most a second for the registry's periodic flusher,
// because per-line flushing on the render path was not the point of this.
static std::shared_ptr<spdlog::logger> open_file_logger(const std::string& path) {
    auto logger = spdlog::basic_logger_mt("term-ime", path, true);
    logger->flush_on(spdlog::level::warn);
    return logger;
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
        auto logger = open_file_logger(log_file);
        spdlog::set_default_logger(logger);
        spdlog::set_level(spdlog::level::debug);
        // Applies to every logger registered afterwards too, so the file the
        // config switches to inherits it.
        spdlog::flush_every(std::chrono::seconds(1));
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
    // Trouble the loader saw travels with the result and is replayed below, once
    // the destination is settled: the log the config chose is the one the user
    // will open, and a message emitted before the switch stayed in the boot log.
    std::vector<std::string> load_notes = config.take_load_notes();

    // The boot logger above exists only so that loading the config could be
    // logged; from here the config owns the level and the file.
    if (file_logging) {
        const std::string boot_file = log_file;
        spdlog::set_level(config_log_level(config.log_level));
        if (!config.log_file.empty() && config.log_file != log_file) {
            try {
                std::filesystem::create_directories(std::filesystem::path(config.log_file).parent_path());
                spdlog::default_logger()->flush();
                spdlog::drop("term-ime");
                spdlog::set_default_logger(open_file_logger(config.log_file));
                log_file = config.log_file;
            } catch (const std::exception& e) {
                // The boot logger is still held by spdlog's default pointer with
                // its file open, so this report has somewhere to go. Turning the
                // level off here -- as it used to -- hid every later warning
                // behind the very failure it was reporting.
                log_file = boot_file;
                spdlog::error("Cannot log to {}: {} -- still logging to {}", config.log_file, e.what(), boot_file);
            }
        }
        spdlog::info("Loaded config from: {}", config_path);
        if (log_file != boot_file)
            spdlog::info("logging to {} instead of {}", log_file, boot_file);
    }
    for (const auto& note : load_notes)
        spdlog::warn("config: {}", note);

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

    // A refused watch means term-ime never sees that fd or signal again -- no
    // keyboard, no shell output, and a screen that sits there looking hung. Say
    // so and leave instead.
    auto refused = [&app](const char* what) {
        spdlog::error("{} watch registration refused, exiting", what);
        std::cerr << "term-ime: cannot register the " << what << " watcher (see the log file)" << std::endl;
        app.on_quit(0);
        return 1;
    };

    // PTY reader: shell output, plus the hangup that ends the session.
    EventLoop::IoCallback pty_reader = [&](const char* data, size_t len) {
        if (len == 0 || data == nullptr) {
            spdlog::info("PTY closed, exiting");
            app.on_quit(0);
            loop.stop();
        } else {
            app.on_pty_data(data, len);
        }
    };
    EventLoop::IoCallback keyboard_reader = [&](const char* data, size_t len) {
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
    };
    EventLoop::SignalCallback quit_session = [&](int signum) {
        app.on_quit(signum);
        loop.stop();
    };

    if (!loop.watch_fd(app.pty_fd(), pty_reader))
        return refused("pty");
    if (!loop.watch_fd(STDIN_FILENO, keyboard_reader))
        return refused("stdin");
    if (!loop.watch_signal(SIGWINCH, [&app](int signum) { app.on_resize(signum); }))
        return refused("SIGWINCH");
    if (!loop.watch_signal(SIGINT, quit_session))
        return refused("SIGINT");
    if (!loop.watch_signal(SIGTERM, quit_session))
        return refused("SIGTERM");

    // Run event loop
    spdlog::info("Starting event loop");
    loop.run();
    spdlog::info("Event loop finished");
    spdlog::default_logger()->flush();

    return 0;
}
