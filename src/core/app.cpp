#include "app.hpp"
#include "event_loop.hpp"
#include <spdlog/spdlog.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <stdexcept>

App::App() = default;

App::~App() {
    try {
        if (initialized_) {
            renderer_.restore();
        }
        // unique_ptr handles cleanup automatically
    } catch (const std::exception& e) {
        spdlog::error("Exception in App destructor: {}", e.what());
    }
}

bool App::init(const AppConfig& config) {
    spdlog::info("App::init starting");
    config_ = config;

    // 检查是否是 TTY
    if (!isatty(STDIN_FILENO)) {
        spdlog::error("Not a TTY, cannot run");
        return false;
    }

    try {
        // Initialize renderer
        spdlog::info("Initializing renderer");
        renderer_.init();

        // Spawn shell in PTY
        spdlog::info("Spawning PTY");
        if (!pty_.spawn(config.shell)) {
            spdlog::error("Failed to spawn shell: {}", config.shell);
            renderer_.restore();
            return false;
        }

        // Get terminal size
        struct winsize ws;
        if (ioctl(renderer_.get_tty_fd(), TIOCGWINSZ, &ws) < 0 ||
            ws.ws_row == 0 || ws.ws_row > 1000 ||
            ws.ws_col == 0 || ws.ws_col > 1000) {
            spdlog::warn("Failed to get terminal size, using defaults");
            ws.ws_row = 24;
            ws.ws_col = 80;
        }

        // Create screen and parser
        spdlog::info("Creating screen {}x{}", ws.ws_row - 1, ws.ws_col);
        screen_ = std::make_unique<Screen>(ws.ws_row - 1, ws.ws_col);
        if (!screen_) {
            spdlog::error("Failed to create screen");
            renderer_.restore();
            return false;
        }

        parser_ = std::make_unique<Parser>(*screen_);
        if (!parser_) {
            spdlog::error("Failed to create parser");
            screen_.reset();
            renderer_.restore();
            return false;
        }

        // Initialize language manager
        spdlog::info("Loading language configuration");
        language_manager_.load(config_);
        language_manager_.on_language_change(
            [this](const LanguageConfig& lang) { on_language_change(lang); });

        // Initialize Rime IME with current language's schema
        const auto& current_lang = language_manager_.current();
        spdlog::info("Initializing Rime IME with schema: {}", current_lang.schema);

        ime_ = std::make_unique<RimeIme>();
        if (!ime_->initialize()) {
            spdlog::warn("Failed to initialize Rime IME, continuing without IME");
        } else {
            ime_->select_schema(current_lang.schema);
            spdlog::info("Rime IME initialized");
        }

        // Initialize LLM ranker if enabled in config
        if (config_.llama_ranker.enabled) {
            spdlog::info("Initializing LLM ranker");
            llama_ranker_ = std::make_unique<LlamaRanker>();
            if (llama_ranker_->initialize(config_.llama_ranker)) {
                ai_ranking_enabled_ = true;
                spdlog::info("LLM ranker initialized");
            } else {
                spdlog::warn("Failed to initialize LLM ranker, continuing without AI ranking");
                llama_ranker_.reset();
            }
        }

        initialized_ = true;
        spdlog::info("App::init complete");
        return true;

    } catch (const std::exception& e) {
        spdlog::error("Exception during init: {}", e.what());
        renderer_.restore();
        return false;
    }
}

void App::on_pty_data(const char* data, size_t len) {
    // 直接转发 PTY 输出到终端，不解析
    renderer_.forward_output(data, len);

    // 同时更新内部屏幕状态
    if (parser_) {
        parser_->feed(reinterpret_cast<const uint8_t*>(data), len);
    }

    // 显示候选词栏
    render_candidates_bar();
}

void App::render_candidates_bar() {
    std::string mode = (ime_ && ime_->mode() == ImeMode::Chinese) ? "中文" : "EN";
    std::string lang_name = language_manager_.current().name;

    // Get candidates from IME
    auto candidates = ime_ ? ime_->candidates() : std::vector<Candidate>();
    std::string buffer = ime_ ? ime_->buffer() : "";

    // Apply AI ranking if enabled and candidates available
    if (ai_ranking_enabled_ && llama_ranker_ && !candidates.empty() && !ai_ranking_in_progress_) {
        // Store candidates for async ranking
        last_candidates_ = candidates;

        // Start async ranking (delayed execution)
        ai_ranking_in_progress_ = true;
        llama_ranker_->rank_async(
            candidates,
            "",  // context (could be from screen)
            buffer,
            [this](std::vector<Candidate> ranked) {
                on_ranking_complete(std::move(ranked));
            }
        );
    }

    // Build status string with AI indicator
    std::string status = lang_name + " " + mode;
    if (ai_ranking_enabled_) {
        status += ai_ranking_in_progress_ ? " [AI...]" : " [AI]";
    }

    renderer_.render_candidates(candidates, selected_candidate_, buffer, status);
}

void App::on_ranking_complete(std::vector<Candidate> ranked) {
    ai_ranking_in_progress_ = false;
    last_candidates_ = ranked;

    // Re-render with ranked candidates
    if (ime_ && ime_->state() != ImeState::Inactive) {
        std::string mode = ime_->mode() == ImeMode::Chinese ? "中文" : "EN";
        std::string lang_name = language_manager_.current().name;
        std::string status = lang_name + " " + mode + " [AI]";

        renderer_.render_candidates(ranked, selected_candidate_, ime_->buffer(), status);
    }
}

void App::on_keyboard_data(const char* data, size_t len) {
    if (len == 0 || !ime_) return;

    // Process each byte through InputProcessor state machine
    for (size_t i = 0; i < len; ++i) {
        uint8_t byte = static_cast<uint8_t>(data[i]);

        auto input_result = input_processor_.process(byte);

        // Handle toggle mode command (Ctrl+A + Space)
        if (input_result.toggle_mode) {
            spdlog::info("Ctrl+A + Space detected, toggling mode");
            ime_->toggle_mode();
            render();
            continue;
        }

        // Handle AI ranking toggle (Ctrl+A + A)
        if (input_result.forward && !input_result.data.empty()) {
            if (input_result.data.size() == 2 && input_result.data[0] == 1 && input_result.data[1] == 'a') {
                spdlog::info("Ctrl+A + A detected, toggling AI ranking");
                toggle_ai_ranking();
                continue;
            }
        }

        // Check if this is an escape sequence
        bool is_escape_sequence = !input_result.data.empty() && input_result.data[0] == 0x1b;

        // If IME is composing, intercept all input except selection keys
        if (ime_->state() == ImeState::Composing || ime_->state() == ImeState::Selecting) {
            // Escape sequences are ignored while composing
            if (is_escape_sequence && input_result.forward) {
                spdlog::debug("IME composing: ignoring escape sequence");
                continue;
            }

            char ch = static_cast<char>(byte);
            if (ch >= '1' && ch <= '9') {
                // Select candidate
                int idx = ch - '1';
                auto candidates = ime_->select(idx);
                if (!candidates.empty()) {
                    std::string utf8;
                    for (char32_t c : candidates) {
                        utf8 += utf8::encode(c);
                    }
                    pty_.write(std::vector<uint8_t>(utf8.begin(), utf8.end()));
                }
                render();
                continue;
            } else if (ch == ' ') {
                // Space selects first candidate
                auto candidates = ime_->select(0);
                if (!candidates.empty()) {
                    std::string utf8;
                    for (char32_t c : candidates) {
                        utf8 += utf8::encode(c);
                    }
                    pty_.write(std::vector<uint8_t>(utf8.begin(), utf8.end()));
                }
                render();
                continue;
            } else if (ch == '\b' || ch == 127) {
                ime_->cancel();
                render();
                continue;
            } else if (ch >= 'a' && ch <= 'z') {
                ime_->input(ch);
                selected_candidate_ = 0;
                render();
                continue;
            }
            // Other keys are ignored while composing
            spdlog::debug("IME composing: ignoring key 0x{:02x}", byte);
            continue;
        }

        // Not composing - check if should start composing
        if (ime_->mode() == ImeMode::Chinese && byte >= 'a' && byte <= 'z' && !input_processor_.in_escape()) {
            ime_->input(static_cast<char>(byte));
            selected_candidate_ = 0;
            render();
            continue;
        }

        // Forward to shell if requested
        if (input_result.forward && !input_result.data.empty()) {
            pty_.write(input_result.data);
        }
    }
}

void App::on_resize(int signum) {
    (void)signum;
    struct winsize ws;
    ioctl(renderer_.get_tty_fd(), TIOCGWINSZ, &ws);

    if (screen_) {
        screen_->resize(ws.ws_row - 1, ws.ws_col);
    }
    pty_.resize(ws.ws_row - 1, ws.ws_col);

    render();
}

void App::on_quit(int signum) {
    (void)signum;
    renderer_.restore();
    initialized_ = false;
}

void App::render() {
    if (!screen_) return;

    spdlog::debug("render: starting");
    renderer_.render(*screen_);
    spdlog::debug("render: screen done");

    render_candidates_bar();
    spdlog::debug("render: candidates done");
}

int App::pty_fd() const {
    return pty_.fd();
}

int App::tty_fd() const {
    return renderer_.get_tty_fd();
}

const LanguageConfig& App::current_language() const {
    return language_manager_.current();
}

bool App::switch_language(const std::string& lang_id) {
    return language_manager_.switch_language(lang_id);
}

void App::toggle_ai_ranking() {
    if (!llama_ranker_) {
        // Initialize ranker on first toggle
        spdlog::info("Initializing LLM ranker on demand");
        llama_ranker_ = std::make_unique<LlamaRanker>();
        if (llama_ranker_->initialize(config_.llama_ranker)) {
            ai_ranking_enabled_ = true;
            spdlog::info("LLM ranker initialized and enabled");
        } else {
            spdlog::warn("Failed to initialize LLM ranker");
            llama_ranker_.reset();
        }
    } else {
        ai_ranking_enabled_ = !ai_ranking_enabled_;
        spdlog::info("AI ranking {}", ai_ranking_enabled_ ? "enabled" : "disabled");
    }
    render();
}

bool App::is_ai_ranking_enabled() const {
    return ai_ranking_enabled_;
}

void App::on_language_change(const LanguageConfig& lang) {
    spdlog::info("Language changed to: {} ({})", lang.name, lang.schema);
    if (ime_ && !lang.schema.empty()) {
        ime_->select_schema(lang.schema);
    }
    render();
}