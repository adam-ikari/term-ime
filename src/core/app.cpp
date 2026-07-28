#include "app.hpp"
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

    try {
        // Initialize renderer
        spdlog::info("Initializing renderer");
        renderer_.init();
        if (!renderer_.is_initialized()) {
            spdlog::error("Renderer initialization failed (not a TTY or alternate screen not supported)");
            return false;
        }

        // Spawn shell in PTY
        spdlog::info("Spawning PTY");
        if (!pty_.spawn(config.shell)) {
            spdlog::error("Failed to spawn shell: {}", config.shell);
            renderer_.restore();
            return false;
        }

        // Get terminal size
        struct winsize ws;
        int tty_fd = renderer_.get_tty_fd();
        if (ioctl(tty_fd, TIOCGWINSZ, &ws) < 0 || ws.ws_row == 0 || ws.ws_row > 1000 ||
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

    // 重绘候选/状态栏。render_candidates 内部对 IME inactive 时的状态栏
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

    renderer_.render_candidates(candidates, selected_candidate_, buffer, mode);
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
                if (second_key == 's') {
                    spdlog::info("Ctrl+A + S detected, toggling settings");
                    // Cancel IME input first
                    if (ime_->state() != ImeState::Inactive) {
                        ime_->cancel();
                    }
                    toggle_settings();
                    continue;
                } else if (second_key == '\x03') {
                    // Ctrl+A + Ctrl+C — quit term-ime
                    spdlog::info("Ctrl+A + Ctrl+C detected, quitting");
                    if (ime_->state() != ImeState::Inactive) {
                        ime_->cancel();
                    }
                    on_quit(0);
                    return;
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
            // ESC (0x1b) cancels composition immediately
            if (ch == 0x1b) {
                spdlog::debug("IME composing: ESC cancels composition");
                ime_->cancel();
                selected_candidate_ = 0;
                render();
                continue;
            }
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
                // Backspace: delete one syllable character, not the whole
                // composition. Send XK_BackSpace to rime so it handles the
                // deletion internally (preserving remaining input).
                ime_->backspace();
                selected_candidate_ = 0;
                render();
                continue;
            } else if (ch >= 'a' && ch <= 'z') {
                bool accepted = ime_->input(ch);
                selected_candidate_ = 0;
                if (accepted) {
                    // 输入被接受，延迟渲染
                    need_render_ = true;
                } else {
                    // 输入未被接受（如无效拼音组合），立即渲染显示当前状态
                    render();
                }
                continue;
            } else if (ch == '\'') {
                // 单引号作为拼音分隔符，传递给 rime 处理
                bool accepted = ime_->input(ch);
                if (accepted) {
                    need_render_ = true;
                } else {
                    render();
                }
                continue;
            } else if (ch == ',' || ch == '<') {
                // 上翻页（逗号/<）
                if (ime_->state() == ImeState::Selecting) {
                    ime_->page_up();
                    render();
                    continue;
                }
            } else if (ch == '.' || ch == '>') {
                // 下翻页（句号/>）
                if (ime_->state() == ImeState::Selecting) {
                    ime_->page_down();
                    render();
                    continue;
                }
            } else if (ch >= 'A' && ch <= 'Z') {
                // 大写字母（Shift+字母）：先把拼音 buffer 作为字母提交，再输出当前字母
                if (is_escape_sequence && input_result.forward) {
                    continue;
                }
                // 获取当前拼音 buffer 并作为普通字母提交
                std::string buf = ime_->buffer();
                if (!buf.empty()) {
                    std::vector<uint8_t> buf_bytes(buf.begin(), buf.end());
                    pty_.write(buf_bytes);
                }
                // 清除 composition
                ime_->cancel();
                selected_candidate_ = 0;
                // 输出当前字母（保持大写，Shift+字母本身就输出大写）
                pty_.write(std::vector<uint8_t>{static_cast<uint8_t>(ch)});
                render();
                continue;
            }
            // Other keys are ignored while composing
            spdlog::debug("IME composing: ignoring key 0x{:02x}", byte);
            continue;
        }

        // Not composing - check if should start composing
        if (ime_->mode() == ImeMode::Chinese && byte >= 'a' && byte <= 'z' && !input_processor_.in_escape()) {
            bool accepted = ime_->input(static_cast<char>(byte));
            selected_candidate_ = 0;
            if (accepted) {
                need_render_ = true;
            } else {
                render();
            }
            continue;
        }

        // 大写字母（Shift+字母）在中文模式下提交 buffer 并直接输出字母
        if (ime_->mode() == ImeMode::Chinese && byte >= 'A' && byte <= 'Z' && !input_processor_.in_escape()) {
            std::string buf = ime_->buffer();
            if (!buf.empty()) {
                std::vector<uint8_t> buf_bytes(buf.begin(), buf.end());
                pty_.write(buf_bytes);
                ime_->cancel();
            }
            pty_.write(std::vector<uint8_t>{static_cast<uint8_t>(byte)});
            continue;
        }

        // Forward to shell if requested
        if (input_result.forward && !input_result.data.empty()) {
            pty_.write(input_result.data);
        }
    }

    // 延迟渲染：处理完所有字节后只渲染一次
    if (need_render_) {
        need_render_ = false;
        render();
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

    need_render_ = false;

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

void App::switch_ui_language(const std::string& lang_code) {
    I18n::Lang new_lang = I18n::parse_lang(lang_code);
    I18n::set_lang(new_lang);
    config_.ui_language = lang_code;
    spdlog::info("UI language switched to: {}", lang_code);
    render();
}

std::vector<std::pair<std::string, std::string>> App::available_ui_languages() {
    return {{"en", "English"}, {"zh-CN", "简体中文"}};
}

void App::toggle_settings() {
    bool was_visible = settings_state_.visible;
    settings_state_.visible = !settings_state_.visible;
    if (settings_state_.visible) {
        ui::settings_init(settings_state_, config_);
    } else if (was_visible && screen_) {
        // The settings panel drew a fullscreen overlay (ESC[2J); restore the
        // shell view from the Screen grid before repainting the status bar,
        // otherwise the stale settings content remains on screen.
        renderer_.redraw_shell(*screen_);
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
    }

    render();
}

void App::on_settings_close() {
    settings_state_.visible = false;
    // Restore the shell view after the fullscreen settings overlay.
    if (screen_) {
        renderer_.redraw_shell(*screen_);
    }
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