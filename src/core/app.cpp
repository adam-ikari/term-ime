#include "app.hpp"
#include "event_loop.hpp"
#include "util/i18n.hpp"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <unistd.h>
#include <sys/ioctl.h>
#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <filesystem>
#include <limits>

// Orphaned-ESC timeout (vi/urxvt-style escape-timeout). A bare ESC sitting in
// the InputProcessor's Escape state for this long with no follow-up byte is
// treated as an independent ESC keypress and forwarded to the shell.
static constexpr uint64_t kEscapeTimeoutMs = 50;

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
bool App::init(const AppConfig& config, EventLoop* event_loop) {
    spdlog::info("App::init starting");
    config_ = config;
    event_loop_ = event_loop;

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
        if (ioctl(tty_fd, TIOCGWINSZ, &ws) < 0 || ws.ws_row < 2 || ws.ws_row > 1000 || ws.ws_col == 0 ||
            ws.ws_col > 1000) {
            spdlog::warn("Failed to get terminal size, using defaults");
            ws.ws_row = 24;
            ws.ws_col = 80;
        }

        // The status bar owns the last row and the scroll region excludes it, so
        // the shell's usable area is rows-1. Pty::spawn sized the child from the
        // real tty but it doesn't know about the status bar, so sync it here —
        // without this the shell laid out for the wrong height (defect 3b).
        pty_.resize(ws.ws_row - 1, ws.ws_col);

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

        // Rime's first run compiles the dictionary (prism/table) before it can
        // accept input, and on a cold data dir that blocks for seconds. Without
        // this the alternate screen stays blank the whole time and every
        // keystroke is silently ignored — which reads as a hung UI.
        auto show_startup_hint = [this](const std::string& text) {
            static const char kClear[] = "\x1b[H\x1b[2K";
            renderer_.forward_output(kClear, sizeof(kClear) - 1);
            if (!text.empty()) {
                renderer_.forward_output(text.data(), text.size());
            }
        };
        show_startup_hint(I18n::t("status.initializing"));

        ime_ = std::make_unique<RimeIme>();
        // Record the fuzzy groups before initialize(): the engine materialises
        // any per-combination schema during init, then the schema selection
        // below picks the right twin.
        ime_->set_fuzzy_groups(config_.fuzzy_groups);
        if (!ime_->initialize()) {
            spdlog::warn("Failed to initialize Rime IME, continuing without IME");
        } else {
            ime_->select_schema(ime_->fuzzy_variant(current_lang.schema));
            spdlog::info("Rime IME initialized");
        }
        // Deploy done: drop the transient hint so the shell prompt owns row 0.
        show_startup_hint(std::string());

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
    // The settings panel is a fullscreen overlay. Shell output must not be
    // painted over it, but it must not be dropped either (defect 8): keep the
    // internal screen model current and let on_settings_close()/
    // toggle_settings() repaint the shell view from it.
    if (settings_state_.visible) {
        if (parser_) {
            parser_->feed(reinterpret_cast<const uint8_t*>(data), len);
        }
        return;
    }

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
    // Shell output cannot change the IME context, so reuse the cached snapshot
    // rather than querying rime three more times (defect 17).
    render_candidates_bar(false);
}

void App::refresh_ime_snapshot() {
    if (!ime_) {
        ime_snapshot_ = ImeSnapshot{};
        return;
    }
    ime_snapshot_.mode = (ime_->mode() == ImeMode::Chinese) ? "拼" : "EN";
    ime_snapshot_.candidates = ime_->candidates();
    ime_snapshot_.buffer = ime_->buffer();
}

void App::render_candidates_bar(bool refresh) {
    if (refresh) {
        refresh_ime_snapshot();
    }

    const std::vector<Candidate>& all = ime_snapshot_.candidates;

    // A different candidate set means a new composition or a new rime page, so
    // the previous window offset no longer refers to anything.
    std::string sig;
    for (const Candidate& c : all) {
        for (char32_t ch : c.text) {
            if (ch != 0)
                sig += utf8::encode(ch);
        }
        sig += '\x1f';
    }
    if (sig != candidate_page_sig_) {
        candidate_page_sig_ = std::move(sig);
        candidate_window_ = 0;
    }

    struct winsize ws {};
    int cols = 80;
    if (ioctl(renderer_.get_tty_fd(), TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0)
        cols = ws.ws_col;

    std::vector<Candidate> shown;
    if (!all.empty()) {
        if (candidate_window_ > all.size() - 1)
            candidate_window_ = all.size() - 1;
        // Fit over the tail of the page, because the bar re-derives the count
        // from exactly the list it is handed: both sides must see the same list
        // or the drawn set and the selectable set would drift apart.
        std::vector<Candidate> tail(all.begin() + candidate_window_, all.end());
        int fit =
            ui::FitCandidateBar(cols, ime_snapshot_.mode, ime_snapshot_.buffer, tail, config_.max_candidates).count;
        if (fit < 1)
            fit = 1;
        if (static_cast<size_t>(fit) >= all.size()) {
            candidate_window_ = 0;
            fit = static_cast<int>(all.size());
        } else if (candidate_window_ > all.size() - static_cast<size_t>(fit)) {
            candidate_window_ = all.size() - static_cast<size_t>(fit);  // keep a full window
        }

        // The window is driven by the user ('.'/','): a highlight that sits
        // outside it must not drag it back, or paging would never advance.

        shown.assign(all.begin() + candidate_window_, all.begin() + candidate_window_ + static_cast<size_t>(fit));
        candidate_slots_ = fit;
    } else {
        candidate_window_ = 0;
        candidate_slots_ = 0;
    }

    // Highlight the first visible candidate when the selection is off-window.
    size_t sel = 0;
    if (!shown.empty() && selected_candidate_ >= candidate_window_ &&
        selected_candidate_ < candidate_window_ + shown.size())
        sel = selected_candidate_ - candidate_window_;

    renderer_.render_candidates(shown, sel, ime_snapshot_.buffer, ime_snapshot_.mode, config_.max_candidates);
}
void App::advance_candidate_window(int direction) {
    if (!ime_)
        return;
    const size_t step = static_cast<size_t>(std::max(1, candidate_slots_));
    if (direction > 0) {
        if (candidate_window_ + step < ime_snapshot_.candidates.size()) {
            candidate_window_ += step;
        } else {
            ime_->page_down();
            candidate_window_ = 0;
        }
    } else if (candidate_window_ >= step) {
        candidate_window_ -= step;
    } else {
        ime_->page_up();
        // Ask for the tail window; render clamps to the last full one.
        candidate_window_ = std::numeric_limits<size_t>::max();
    }
}

void App::on_keyboard_data(const char* data, size_t len) {
    if (len == 0 || !ime_)
        return;

    // Any new input supersedes a pending lone-ESC timeout: if this batch
    // completes the escape sequence (ESC[A etc.) everything proceeds normally;
    // if it ends with a fresh lone ESC, the timer is re-armed below. (When a
    // timer is armed, esc_timer_id_ != 0, which implies event_loop_ != null.)
    if (esc_timer_id_ != 0) {
        event_loop_->clear_timer(esc_timer_id_);
        esc_timer_id_ = 0;
    }

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
            // That ESC was consumed as the panel's cancel key, so drop the
            // residual escape state: otherwise the next key gets glued onto it
            // and never reaches the panel/shell as a key of its own (defect 10).
            input_processor_.reset();
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
            // Ctrl+C / Ctrl+D / Ctrl+Z must still reach the shell while a
            // composition is in progress: cancel the pinyin and pass the byte
            // straight through (it is a plain control byte, not an escape).
            if (byte == 0x03 || byte == 0x04 || byte == 0x1a) {
                if (ime_->state() != ImeState::Inactive) {
                    ime_->cancel();
                }
                selected_candidate_ = 0;
                pty_.write(std::vector<uint8_t>{byte});
                render();
                continue;
            }
            // Arrow keys and PageUp/PageDown page through the candidates while
            // composing (same grouping as ','/'.'), so the whole page stays
            if (is_escape_sequence && input_result.forward) {
                const auto& seq = input_result.data;  // vector<unsigned char>
                int direction = 0;
                if (seq.size() == 3 && (seq[1] == '[' || seq[1] == 'O')) {
                    switch (seq[2]) {
                    case 'D':  // left
                    case 'A':  // up
                        direction = -1;
                        break;
                    case 'C':  // right
                    case 'B':  // down
                        direction = 1;
                        break;
                    default:
                        break;
                    }
                } else if (seq.size() == 4 && seq[1] == '[' && seq[3] == '~') {
                    if (seq[2] == '5') {  // PageUp
                        direction = -1;
                    } else if (seq[2] == '6') {  // PageDown
                        direction = 1;
                    }
                }
                if (direction == 0) {
                    spdlog::debug("IME composing: ignoring escape sequence");
                    continue;
                }
                if (ime_->state() == ImeState::Selecting) {
                    advance_candidate_window(direction);
                }
                render();
                continue;
            }

            char ch = static_cast<char>(byte);
            if (ch >= '1' && ch <= '9') {
                // Select the candidate in the visible slot: the slot index maps
                // to rime's page through the window offset, so a narrow bar can
                // never select something the user does not see.
                int slot = ch - '1';
                if (slot >= candidate_slots_) {
                    spdlog::debug("Candidate slot {} beyond the {} shown", slot + 1, candidate_slots_);
                    continue;
                }
                auto committed = ime_->select(static_cast<int>(candidate_window_) + slot);
                if (!committed.empty()) {
                    std::string utf8;
                    for (char32_t c : committed) {
                        utf8 += utf8::encode(c);
                    }
                    pty_.write(std::vector<uint8_t>(utf8.begin(), utf8.end()));
                }
                render();
                continue;
            } else if (ch == ' ') {
                // Space selects the first visible candidate.
                auto committed = ime_->select(static_cast<int>(candidate_window_));
                if (!committed.empty()) {
                    std::string utf8;
                    for (char32_t c : committed) {
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
                // Previous group of candidates.
                if (ime_->state() == ImeState::Selecting) {
                    advance_candidate_window(-1);
                    render();
                    continue;
                }
            } else if (ch == '.' || ch == '>') {
                // Next group of candidates.
                if (ime_->state() == ImeState::Selecting) {
                    advance_candidate_window(1);
                    render();
                    continue;
                }
            }
            // Other keys are ignored while composing
            spdlog::debug("IME composing: ignoring key 0x{:02x}", byte);
            continue;
        }

        // Not composing - check if should start composing. A byte that just
        // completed an escape sequence (e.g. the trailing 'c' of a DA reply
        // ESC[?1;2c) is not a pinyin keystroke even though it is a lowercase
        // letter: the SM is back in Normal by the time this byte is examined,
        // so in_escape() alone cannot tell — the forwarded result beginning
        // with ESC is the reliable signal. The sequence is forwarded below.
        bool completed_escape = input_result.forward && !input_result.data.empty() && input_result.data[0] == 0x1b;
        if (ime_->mode() == ImeMode::Chinese && byte >= 'a' && byte <= 'z' && !completed_escape &&
            !input_processor_.in_escape()) {
            bool accepted = ime_->input(static_cast<char>(byte));
            selected_candidate_ = 0;
            if (accepted) {
                need_render_ = true;
            } else {
                render();
            }
            continue;
        }

        // Forward to shell if requested
        if (input_result.forward && !input_result.data.empty()) {
            pty_.write(input_result.data);
        }
    }

    // A lone ESC (no sequence byte followed it in this batch) cancels the
    // composition. A completed sequence (ESC[C …) was handled as candidate
    // paging above and leaves the state machine in Normal, so it cannot land
    // here — checking the byte instead of the batch used to swallow arrows.
    if (input_processor_.in_escape() && ime_ && ime_->state() != ImeState::Inactive) {
        spdlog::debug("IME composing: ESC cancels composition");
        ime_->cancel();
        selected_candidate_ = 0;
        input_processor_.reset();
        render();
    }

    // Orphaned-ESC disambiguation: a bare ESC is still pending in the state
    // machine (not consumed by composition-cancel, settings panel not visible,
    // no sequence byte followed). Arm a short single-shot timer; when it fires
    // the ESC is forwarded to the shell as an independent keypress. If the
    // user's next key completes the sequence first, the timer is cancelled at
    // the top of the next on_keyboard_data call.
    if (event_loop_ && !settings_state_.visible && input_processor_.in_escape()) {
        esc_timer_id_ = event_loop_->set_timer(
            [this]() {
                spdlog::debug("Orphaned-ESC timeout: forwarding lone ESC");
                input_processor_.reset();
                pty_.write(std::vector<uint8_t>{0x1b});
                // Reap this fired single-shot timer's handle so it does not
                // linger in EventLoop::timers_ until shutdown. clear_timer on
                // a fired/unknown id is a safe no-op; calling it from inside
                // the callback is safe (uv_close is processed asynchronously).
                if (esc_timer_id_ != 0) {
                    uint64_t tid = esc_timer_id_;
                    esc_timer_id_ = 0;
                    if (event_loop_) {
                        event_loop_->clear_timer(tid);
                    }
                }
            },
            kEscapeTimeoutMs, false);
    }

    // 延迟渲染：处理完所有字节后只渲染一次
    if (need_render_) {
        need_render_ = false;
        render();
    }
}

void App::on_resize(int signum) {
    (void)signum;

    struct winsize ws {};
    if (ioctl(renderer_.get_tty_fd(), TIOCGWINSZ, &ws) < 0) {
        spdlog::debug("on_resize: TIOCGWINSZ failed: {}, keeping current size", strerror(errno));
        return;
    }
    if (ws.ws_row < 2 || ws.ws_row > 1000 || ws.ws_col == 0 || ws.ws_col > 1000) {
        spdlog::debug("on_resize: ignoring bogus terminal size {}x{}", ws.ws_row, ws.ws_col);
        return;
    }

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
    // Cancel any pending lone-ESC timeout so it cannot fire after teardown.
    if (event_loop_ && esc_timer_id_ != 0) {
        event_loop_->clear_timer(esc_timer_id_);
        esc_timer_id_ = 0;
    }
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
    } else if (key == "max_candidates") {
        // The settings row offers the single-digit options "1".."9".
        const int requested = value.empty() ? 0 : value[0] - '0';
        config_.max_candidates = std::max(1, std::min(9, requested));
        candidate_window_ = 0;
        spdlog::info("Candidate cap set to {}", config_.max_candidates);
    } else if (key.rfind("fuzzy_", 0) == 0) {
        // One of the per-group fuzzy toggles: fuzzy_zh_z / fuzzy_n_l / fuzzy_r /
        // fuzzy_hu_f / fuzzy_nose. The engine maps the resulting group set to a
        // bundled or generated schema (see RimeIme::fuzzy_variant).
        const std::string group = key.substr(6);
        auto& groups = config_.fuzzy_groups;
        auto it = std::find(groups.begin(), groups.end(), group);
        const bool on = (value == "on");
        if (on && it == groups.end()) {
            groups.push_back(group);
        } else if (!on && it != groups.end()) {
            groups.erase(it);
        }
        if (ime_) {
            ime_->set_fuzzy_groups(groups);
            ime_->select_schema(ime_->fuzzy_variant(language_manager_.current().schema));
        }
        candidate_window_ = 0;
        spdlog::info("Fuzzy group {} {}", group, on ? "on" : "off");
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
    if (config_.save(AppConfig::default_path())) {
        spdlog::info("Settings saved to: {}", AppConfig::default_path());
    } else {
        spdlog::error("Failed to save settings to: {}", AppConfig::default_path());
    }
    render();
}

void App::on_language_change(const LanguageConfig& lang) {
    spdlog::info("Language changed to: {} ({})", lang.name, lang.schema);
    if (ime_ && !lang.schema.empty()) {
        ime_->select_schema(lang.schema);
    }
    render();
}