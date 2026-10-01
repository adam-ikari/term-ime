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

// Bytes that librime's punctuator may map to a full-width form (see
// data/rime-data/default.yaml: punctuator.full_shape). Letters and digits are
// excluded: they belong to the speller/selector. This is only a cheap filter —
// rime is still the authority, and an unmapped byte is refused (see ime_feed).
static bool is_punct_key(char ch) {
    switch (ch) {
        case ',':
        case '.':
        case '<':
        case '>':
        case '/':
        case '?':
        case ';':
        case ':':
        case '\'':
        case '"':
        case '\\':
        case '|':
        case '`':
        case '~':
        case '!':
        case '@':
        case '#':
        case '%':
        case '$':
        case '^':
        case '&':
        case '*':
        case '(':
        case ')':
        case '-':
        case '_':
        case '+':
        case '=':
        case '[':
        case ']':
        case '{':
        case '}':
            return true;
        default:
            return false;
    }
}

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

        ime_ = std::make_unique<RimeIme>(config_.rime_shared_data_dir, config_.rime_user_data_dir);
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
    // The shell just consumed enough input to produce output, so its side of the
    // pty has room again: retry whatever an earlier write had to buffer. This is
    // the output hot path, so the attempt is non-blocking (budget 0) -- one
    // write() syscall, and anything the kernel still refuses stays queued.
    pty_.flush();
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

void App::queue_for_shell(const std::vector<uint8_t>& bytes) {
    // The one and only way bytes reach the child. Appending preserves arrival
    // order by construction, and flush_tx_batch() writes the batch in that
    // order — so a commit can never overtake the paste that preceded it, which
    // is the whole reason the immediate-write variant used to exist.
    //
    // There is no "write it now" path on purpose. Two ways to emit bytes meant
    // every call site had to know which one it was, and picking wrong is silent
    // (a reordered stream, not a crash). The one caller that genuinely runs
    // outside a keyboard batch — the orphaned-ESC timer — calls this and then
    // flushes, which is one extra line instead of a permanent second mechanism.
    tx_batch_.insert(tx_batch_.end(), bytes.begin(), bytes.end());
}

void App::flush_tx_batch() {
    if (tx_batch_.empty())
        return;
    // Hand the bytes over by value: from here on they belong to the pty queue,
    // not to the accumulator, so tx_batch_ is empty for the next read.
    std::vector<uint8_t> batch;
    batch.swap(tx_batch_);
    write_to_pty(batch);
}

void App::write_to_pty(const std::vector<uint8_t>& bytes) {
    if (bytes.empty())
        return;
    if (!pty_.write(bytes)) {
        // Pty::write keeps whatever the kernel refused and puts it ahead of the
        // next batch, so this is a delay rather than a loss -- unless the queue
        // itself overflowed, which pty.cpp logs at error level.
        spdlog::debug("pty write incomplete for {} byte(s), buffered for retry", bytes.size());
    }
}

void App::queue_committed(const std::u32string& text) {
    if (text.empty())
        return;
    std::string utf8;
    for (char32_t c : text) {
        utf8 += utf8::encode(c);
    }
    queue_for_shell(std::vector<uint8_t>(utf8.begin(), utf8.end()));
}

bool App::ime_feed(char ch) {
    if (!ime_)
        return false;
    if (!ime_->input(ch))
        return false;
    // Rain / selectless commit: librime can emit text as a direct side effect
    // of a key (full-width punctuation, auto-commit). Forward it now; the
    // composition state is irrelevant once the bytes are out.
    queue_committed(ime_->take_commit());
    return true;
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
    // Read the candidate count from rime directly. It used to come from
    // ime_snapshot_, which only render() refreshes — an implicit dependency
    // that made paging wrong the moment renders were batched, and would have
    // silently paged against a stale candidate list.
    refresh_ime_snapshot();
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

// Which way a completed escape sequence pages the candidate list: -1 previous,
// +1 next, 0 not a paging key. Both ANSI (ESC [ …) and application (ESC O …)
// cursor mode are accepted, matching what the settings panel handles.
static int sequence_paging_direction(const std::vector<uint8_t>& seq) {
    if (seq.size() == 3 && (seq[1] == '[' || seq[1] == 'O')) {
        switch (seq[2]) {
        case 'A':  // up
        case 'D':  // left
            return -1;
        case 'B':  // down
        case 'C':  // right
            return 1;
        default:
            return 0;
        }
    }
    if (seq.size() == 4 && seq[1] == '[' && seq[3] == '~') {
        if (seq[2] == '5')
            return -1;  // PageUp
        if (seq[2] == '6')
            return 1;  // PageDown
    }
    return 0;
}

// Dispatch one byte while a composition is up. Caller has already dealt with
// escape sequences as a unit and with Ctrl+C/D/Z, so this sees plain bytes.
//
// The single rule every branch obeys: a key the IME does not claim belongs to
// the PTY. Every swallowed-input bug fixed here was a branch that quietly
// continued past a key it had not handled — `[7` lost its digit, and Delete
// committed the user's half-typed pinyin while injecting a literal `[3~`. So a
// branch that cannot claim a key returns Forward instead of dropping it, and
// Drop is reserved for the one case where dropping is the right answer.
App::KeyClaim App::handle_composing_key(uint8_t byte) {
    const char ch = static_cast<char>(byte);

    if (ch >= '1' && ch <= '9') {
        // The slot index maps to rime's page through the window offset, so a
        // narrow bar can never select something the user does not see. Past the
        // last slot there is nothing to select, so the digit is not ours.
        const int slot = ch - '1';
        if (slot >= candidate_slots_) {
            spdlog::debug("Candidate slot {} beyond the {} shown", slot + 1, candidate_slots_);
            return KeyClaim::Forward;
        }
        auto committed = ime_->select(candidate_window_ + slot);
        selected_candidate_ = 0;
        if (committed.empty())
            return KeyClaim::Forward;  // rime took nothing, so we did not use it
        queue_committed(committed);
        return KeyClaim::Consumed;
    }

    if (ch == ' ') {
        auto committed = ime_->select(static_cast<int>(candidate_window_));
        if (committed.empty())
            return KeyClaim::Forward;
        queue_committed(committed);
        return KeyClaim::Consumed;
    }

    if (ch == '\r' || ch == '\n') {
        // Enter confirms the composition — the gesture every other terminal IME
        // has. It used to fall through to the generic forwarding branch, so the
        // pinyin buffer was never committed and the shell ran an empty line; and
        // because a digit-select left the composition open, the next Enter
        // committed it a second time, making the shell try to execute 「你好」.
        //
        // Only claim the key when a composition is actually up — otherwise Enter
        // must reach the shell untouched, or running commands in Chinese mode
        // would stop working. That is why this returns Forward rather than
        // committing unconditionally.
        queue_committed(ime_->select(static_cast<int>(candidate_window_) + selected_candidate_));
        // A commit can leave rime reporting Composing with an empty buffer;
        // without this the next Enter commits the stale context a second time.
        ime_->cancel();
        selected_candidate_ = 0;
        return KeyClaim::Consumed;
    }

    if (ch == '\b' || ch == 127) {
        // Delete one syllable character, not the whole composition. XK_BackSpace
        // lets rime do it internally so the remaining input survives.
        ime_->backspace();
        selected_candidate_ = 0;
        return KeyClaim::Consumed;
    }

    if ((ch >= 'a' && ch <= 'z') || ch == '\'') {
        // ' is the pinyin syllable separator (ni'hao); rime declines it when the
        // composition cannot use it, and then it is not ours.
        selected_candidate_ = 0;
        return ime_->input(ch) ? KeyClaim::Consumed : KeyClaim::Forward;
    }

    if (ch == ',' || ch == '<' || ch == '.' || ch == '>') {
        // Selecting: page by one group (same grouping as the arrow keys).
        // Composing: punctuation, so librime's punctuator commits the full-width
        // form instead.
        if (ime_->state() == ImeState::Selecting) {
            advance_candidate_window(ch == ',' || ch == '<' ? -1 : 1);
            return KeyClaim::Consumed;
        }
        return ime_feed(ch) ? KeyClaim::Consumed : KeyClaim::Forward;
    }

    if (is_punct_key(ch)) {
        // Punctuation mid-composition goes through librime's punctuator so the
        // full-width form is committed while the composition stays up. A byte
        // rime does not map is dropped rather than leaked to the shell, which
        // would splice a stray character into the middle of a half-typed word —
        // the one place here where discarding input is deliberate.
        return ime_feed(ch) ? KeyClaim::Consumed : KeyClaim::Drop;
    }

    spdlog::debug("IME composing: ignoring key 0x{:02x}", byte);
    return KeyClaim::Drop;
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
                    flush_tx_batch();  // bytes typed before the quit chord still go out
                    on_quit(0);
                    return;
                }
            }
        }

        // While a composition is up the IME owns the keyboard. Two things are
        // handled out here rather than in handle_composing_key, because they are
        // about the byte stream rather than about the IME:
        if (ime_->state() == ImeState::Composing || ime_->state() == ImeState::Selecting) {
            // Ctrl+C / Ctrl+D / Ctrl+Z must still reach the shell mid-composition:
            // cancel the pinyin and pass the byte straight through.
            if (byte == 0x03 || byte == 0x04 || byte == 0x1a) {
                ime_->cancel();
                selected_candidate_ = 0;
                queue_for_shell(std::vector<uint8_t>{byte});
                need_render_ = true;
                continue;
            }

            // An escape sequence has to be handled as ONE unit. Dispatch is per
            // byte, and when the leading ESC arrives the state machine has not
            // completed anything yet — so reading bytes one at a time dropped the
            // ESC and then offered the remainder to the IME individually. Delete
            // (ESC [ 3 ~) thereby committed a 「 *and* typed a literal `[3~` into
            // the shell. Mid-sequence bytes are swallowed here: the IME must not
            // see any part of a sequence it has not been asked about yet.
            if (input_processor_.in_escape()) {
                continue;
            }
            if (is_escape_sequence && input_result.forward) {
                const int direction = sequence_paging_direction(input_result.data);
                if (direction != 0) {
                    // Arrow keys and PageUp/PageDown page through the candidates
                    // while composing (same grouping as ','/'.'), so the whole
                    // page stays reachable.
                    if (ime_->state() == ImeState::Selecting) {
                        advance_candidate_window(direction);
                    }
                } else {
                    // Home/End/Delete/Insert/F-keys and the like: not ours, so
                    // the shell gets the sequence verbatim rather than it being
                    // dropped or torn apart by the IME.
                    queue_for_shell(input_result.data);
                }
                need_render_ = true;
                continue;
            }

            // Everything else is one plain byte; the handler names what became of
            // it so nothing is dropped by accident.
            switch (handle_composing_key(byte)) {
                case KeyClaim::Consumed:
                    need_render_ = true;
                    break;
                case KeyClaim::Forward:
                    queue_for_shell(std::vector<uint8_t>{byte});
                    need_render_ = true;
                    break;
                case KeyClaim::Drop:
                    break;
            }
            continue;
        }

        // Not composing - check if should start composing. A byte that just
        // completed an escape sequence (e.g. the trailing 'c' of a DA reply
        // ESC[?1;2c) is not a pinyin keystroke even though it is a lowercase
        // letter: the SM is back in Normal by the time this byte is examined,
        // so in_escape() alone cannot tell — the forwarded result beginning
        // with ESC is the reliable signal. The sequence is forwarded below.
        bool completed_escape = input_result.forward && !input_result.data.empty() && input_result.data[0] == 0x1b;
        const bool chinese_ime_key = ime_->mode() == ImeMode::Chinese && !completed_escape &&
                                     !input_processor_.in_escape() && is_punct_key(static_cast<char>(byte));
        if (ime_->mode() == ImeMode::Chinese && byte >= 'a' && byte <= 'z' && !completed_escape &&
            !input_processor_.in_escape()) {
            selected_candidate_ = 0;
            need_render_ = ime_feed(static_cast<char>(byte));
            continue;
        }
        if (chinese_ime_key) {
            // Punctuation in Chinese mode belongs to librime's punctuator, which
            // commits the full-width form (',' → '，', '?' → '？'). rime only
            // reports the key as accepted; the text arrives via take_commit().
            // An unmapped byte is not ours: fall through and hand it to the PTY.
            if (ime_feed(static_cast<char>(byte))) {
                continue;
            }
        }

        // Forward to shell if requested. Accumulated rather than written per
        // byte: a paste is one batch of kilobytes, and per-byte writes each
        // retried the whole outbound queue.
        if (input_result.forward && !input_result.data.empty()) {
            queue_for_shell(input_result.data);
        }
    }
    flush_tx_batch();

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
                // Queued, then flushed: this runs outside a keyboard batch, so
                // nothing else will flush for us.
                queue_for_shell(std::vector<uint8_t>{0x1b});
                flush_tx_batch();
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