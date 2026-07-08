#include "app.hpp"
#include "event_loop.hpp"
#include "util/i18n.hpp"
#include <spdlog/spdlog.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <stdexcept>
#include <filesystem>

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

    // Initialize i18n with UI language from config
    I18n::Lang ui_lang = I18n::parse_lang(config_.ui_language);
    I18n::init(ui_lang);
    spdlog::info("UI language initialized: {}", config_.ui_language);

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
        if (ioctl(renderer_.get_tty_fd(), TIOCGWINSZ, &ws) < 0 || ws.ws_row == 0 || ws.ws_row > 1000 ||
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
        language_manager_.on_language_change([this](const LanguageConfig& lang) { on_language_change(lang); });

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

        // Initialize settings panel
        ui::settings_init(settings_state_, config_);
        settings_state_.on_change = [this](const std::string& key, const std::string& value) {
            on_settings_change(key, value);
        };
        settings_state_.on_close = [this]() { on_settings_close(); };

        initialized_ = true;
        // Paint the initial status bar once; on_pty_data skips repainting while
        // the IME is inactive (F4), so without this the bar wouldn't appear
        // until the user starts composing.
        render_candidates_bar();
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

    // 重绘候选/状态栏。render_candidates_ex 内部对 IME inactive 时的状态栏
    // 做签名去重:shell 高频逐字节回显(如 zle 逐字符回显、注入转义序列被
    // forward)触发的重复重绘会被跳过,而状态栏真正被 shell 清屏擦除时仍会
    // 重绘恢复(monkey 发现 F4)。scroll region(init 的 DECSTBM)进一步保证
    // shell 输出不落 到状态栏行。
    render_candidates_bar();
}

void App::render_candidates_bar() {
    std::string mode = (ime_ && ime_->mode() == ImeMode::Chinese) ? "中文" : "EN";

    // Get candidates from IME
    auto candidates = ime_ ? ime_->candidates() : std::vector<Candidate>();
    std::string buffer = ime_ ? ime_->buffer() : "";

    // Every new candidate batch gets a fresh version, regardless of whether a
    // new ranking is kicked off. If a ranking is already in flight, the in-
    // progress one keeps its old captured version, so its callback will be
    // discarded by on_ranking_complete when it arrives after newer input
    // (monkey finding F5).
    uint64_t version = ++candidate_version_;

    // Apply AI ranking if enabled and candidates available
    if (ai_ranking_enabled_ && llama_ranker_ && !candidates.empty() && !ai_ranking_in_progress_) {
        // Store candidates for async ranking
        last_candidates_ = candidates;

        // Start async ranking (delayed execution)
        ai_ranking_in_progress_ = true;
        llama_ranker_->rank_async(candidates,
                                  "",  // context (could be from screen)
                                  buffer, [this, version](std::vector<Candidate> ranked) {
                                      on_ranking_complete(std::move(ranked), version);
                                  });
    }

    // Use extended render with AI status
    renderer_.render_candidates_ex(candidates, selected_candidate_, buffer, mode, ai_ranking_enabled_,
                                   ai_ranking_in_progress_, model_downloading_.load(), model_download_progress_);
}

void App::on_ranking_complete(std::vector<Candidate> ranked, uint64_t version) {
    ai_ranking_in_progress_ = false;

    // Discard stale results: if a newer candidate batch has been produced
    // since this ranking was kicked off, applying the old ranking would
    // regress the candidate bar to an earlier input (monkey finding F5).
    if (version != candidate_version_.load()) {
        spdlog::debug("Discarding stale ranking result (version {} != {})", version, candidate_version_.load());
        return;
    }

    last_candidates_ = ranked;

    // Re-render with ranked candidates
    if (ime_ && ime_->state() != ImeState::Inactive) {
        std::string mode = ime_->mode() == ImeMode::Chinese ? "中文" : "EN";
        std::string buffer = ime_->buffer();

        renderer_.render_candidates_ex(ranked, selected_candidate_, buffer, mode,
                                       true,   // ai_enabled
                                       false,  // ai_loading (ranking complete)
                                       false,  // downloading
                                       0);
    }
}

void App::on_keyboard_data(const char* data, size_t len) {
    if (len == 0 || !ime_)
        return;

    // If settings panel is visible, handle keys for it
    if (settings_state_.visible) {
        // Process through InputProcessor to handle escape sequences
        for (size_t i = 0; i < len; ++i) {
            uint8_t byte = static_cast<uint8_t>(data[i]);
            auto input_result = input_processor_.process(byte);

            if (input_result.forward && !input_result.data.empty()) {
                // Handle escape sequences (arrow keys)
                // Both ESC [ A/B/C/D (ANSI mode) and ESC O A/B/C/D (application mode)
                if (input_result.data.size() == 3 && input_result.data[0] == 0x1b &&
                    (input_result.data[1] == '[' || input_result.data[1] == 'O')) {
                    // Arrow key: ESC [ A/B/C/D or ESC O A/B/C/D
                    char arrow = static_cast<char>(input_result.data[2]);
                    ui::settings_handle_key(settings_state_, arrow);
                } else {
                    // Forward other keys as individual bytes
                    for (uint8_t b : input_result.data) {
                        ui::settings_handle_key(settings_state_, b);
                    }
                }
            }
        }
        // A lone ESC is swallowed by the SML into the Escape state (forward is
        // never set), so settings_handle_key never receives 0x1b and the panel
        // can't be closed with a single ESC despite the UI hint "取消: Esc/Tab".
        // Arrow keys (ESC[A etc.) complete and forward, leaving the SM back in
        // Normal — so if we're still mid-escape after processing this batch, it
        // is a genuine lone ESC: close the panel (monkey finding F3).
        if (settings_state_.visible && input_processor_.in_escape()) {
            ui::settings_handle_key(settings_state_, 0x1b);
        }
        render();
        return;
    }

    // Process each byte through InputProcessor state machine
    for (size_t i = 0; i < len; ++i) {
        uint8_t byte = static_cast<uint8_t>(data[i]);

        auto input_result = input_processor_.process(byte);

        // Handle toggle mode command (Ctrl+A + Space)
        if (input_result.toggle_mode) {
            spdlog::info("Ctrl+A + Space detected, toggling mode");
            // Cancel any in-progress composition first, so the mode switch is
            // clean — otherwise the composing-intercept below keeps swallowing
            // input even after switching to English (monkey finding F1).
            if (ime_->state() != ImeState::Inactive) {
                ime_->cancel();
            }
            ime_->toggle_mode();
            render();
            continue;
        }

        // Check if this is an escape sequence
        bool is_escape_sequence = !input_result.data.empty() && input_result.data[0] == 0x1b;

        // Check for Ctrl+A combinations first (even when composing)
        if (input_result.forward && !input_result.data.empty()) {
            if (input_result.data.size() == 2 && input_result.data[0] == 1) {
                // Ctrl+A + key combinations
                char second_key = static_cast<char>(input_result.data[1]);
                if (second_key == 'a') {
                    spdlog::info("Ctrl+A + A detected, toggling AI ranking");
                    // Cancel IME input first
                    if (ime_->state() != ImeState::Inactive) {
                        ime_->cancel();
                    }
                    toggle_ai_ranking();
                    continue;
                } else if (second_key == 's') {
                    spdlog::info("Ctrl+A + S detected, toggling settings");
                    // Cancel IME input first
                    if (ime_->state() != ImeState::Inactive) {
                        ime_->cancel();
                    }
                    toggle_settings();
                    continue;
                }
            }
        }

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

    // Re-establish the scroll region for the new size so shell output stays out
    // of the status-bar row (monkey finding F4).
    renderer_.update_scroll_region();

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
    if (!screen_)
        return;

    spdlog::debug("render: starting");
    renderer_.render(*screen_);
    spdlog::debug("render: screen done");

    // Render settings panel if visible
    if (settings_state_.visible) {
        renderer_.render_settings(settings_state_);
        spdlog::debug("render: settings panel done");
    } else {
        render_candidates_bar();
        spdlog::debug("render: candidates done");
    }
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
    if (model_downloading_.load()) {
        spdlog::info("Model download in progress, ignoring toggle");
        return;
    }

    if (!llama_ranker_) {
        // Check if model exists
        std::string model_path = config_.llama_ranker.model_path;
        if (model_path.empty()) {
            model_path = ModelDownloader::get_default_save_path("qwen-0.5b-q4");
        }

        // Expand ~ in path
        if (!model_path.empty() && model_path[0] == '~') {
            const char* home = std::getenv("HOME");
            if (home) {
                model_path = std::string(home) + model_path.substr(1);
            }
        }

        // Check if model file exists
        bool model_exists = std::filesystem::exists(model_path);
        spdlog::info("Model path: {}, exists: {}", model_path, model_exists);

        if (!model_exists) {
            // Start model download
            start_model_download();
            return;
        }

        // Initialize ranker with existing model
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

void App::start_model_download() {
    spdlog::info("Starting model download");
    model_downloading_ = true;
    model_download_progress_ = 0;

    if (!model_downloader_) {
        model_downloader_ = std::make_unique<ModelDownloader>();
    }

    std::string model_name = "qwen-0.5b-q4";  // Default model
    std::string save_path = config_.llama_ranker.model_path;
    if (save_path.empty()) {
        save_path = ModelDownloader::get_default_save_path(model_name);
    }

    model_downloader_->download_async(
        model_name, save_path,
        [this](int progress, const std::string& status) {
            model_download_progress_ = progress;
            spdlog::info("Download progress: {}% - {}", progress, status);
            render();
        },
        [this](bool success, const std::string& path) { on_model_downloaded(success, path); });

    render();
}

void App::on_model_downloaded(bool success, const std::string& path) {
    model_downloading_ = false;

    if (success) {
        spdlog::info("Model downloaded successfully: {}", path);

        // Update config with downloaded model path
        config_.llama_ranker.model_path = path;
        // The user explicitly requested AI ranking (Ctrl+A+A) and just waited
        // for a download; flip enabled on so initialize() doesn't reject the
        // very model we just fetched (monkey finding F7).
        config_.llama_ranker.enabled = true;

        // Initialize ranker
        llama_ranker_ = std::make_unique<LlamaRanker>();
        if (llama_ranker_->initialize(config_.llama_ranker)) {
            ai_ranking_enabled_ = true;
            spdlog::info("LLM ranker initialized with downloaded model");
        } else {
            spdlog::warn("Failed to initialize LLM ranker with downloaded model");
            llama_ranker_.reset();
        }
    } else {
        spdlog::error("Model download failed");
    }

    render();
}

bool App::is_ai_ranking_enabled() const {
    return ai_ranking_enabled_;
}

void App::switch_ui_language(const std::string& lang_code) {
    I18n::Lang new_lang = I18n::parse_lang(lang_code);
    I18n::set_lang(new_lang);
    config_.ui_language = lang_code;
    spdlog::info("UI language switched to: {}", lang_code);
    render();
}

std::vector<std::pair<std::string, std::string>> App::available_ui_languages() {
    return {{"en", "English"}, {"zh-CN", "简体中文"}, {"zh-TW", "繁體中文"}, {"ja", "日本語"}};
}

void App::toggle_settings() {
    settings_state_.visible = !settings_state_.visible;
    if (settings_state_.visible) {
        ui::settings_init(settings_state_, config_);
    }
    render();
}

bool App::is_settings_visible() const {
    return settings_state_.visible;
}

void App::on_settings_change(const std::string& key, const std::string& value) {
    spdlog::info("Settings changed: {} = {}", key, value);

    if (key == "ui_language") {
        I18n::Lang lang = I18n::parse_lang(value);
        I18n::set_lang(lang);
        config_.ui_language = value;
        // Re-init settings to update labels
        ui::settings_init(settings_state_, config_);
    } else if (key == "ai_ranking") {
        config_.llama_ranker.enabled = (value == "on");
    } else if (key == "backend") {
        config_.llama_ranker.backend = value;
    } else if (key == "threads") {
        config_.llama_ranker.n_threads = std::stoi(value);
    }

    render();
}

void App::on_settings_close() {
    settings_state_.visible = false;
    // Save config to file
    config_.save(AppConfig::default_path());
    spdlog::info("Settings saved to: {}", AppConfig::default_path());
    render();
}

void App::on_language_change(const LanguageConfig& lang) {
    spdlog::info("Language changed to: {} ({})", lang.name, lang.schema);
    if (ime_ && !lang.schema.empty()) {
        ime_->select_schema(lang.schema);
    }
    render();
}