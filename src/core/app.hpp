#pragma once

#include "terminal/pty.hpp"
#include "terminal/screen.hpp"
#include "terminal/parser.hpp"
#include "ime/rime_engine.hpp"
#include "ime/language.hpp"
#include "ui/renderer.hpp"
#include "ui/settings.hpp"
#include "input_processor.hpp"
#include "config.hpp"
#include "util/utf8.hpp"

#include <memory>
#include <string>
#include <atomic>

// Application state
class App {
   public:
    App();
    ~App();

    // Initialize application with config
    bool init(const AppConfig& config);

    // Handle PTY data
    void on_pty_data(const char* data, size_t len);

    // Handle keyboard data
    void on_keyboard_data(const char* data, size_t len);

    // Handle window resize signal
    void on_resize(int signum);

    // Handle quit signal / request
    void on_quit(int signum);

    // True after on_quit() was called (e.g. Ctrl+A+Ctrl+C) — the caller should
    // stop the event loop.
    bool quit_requested() const { return !initialized_; }

    // Render current state
    void render();

    // Get PTY fd for event loop
    int pty_fd() const;

    // Get TTY fd for event loop
    int tty_fd() const;

    // Get current language
    const LanguageConfig& current_language() const;

    // Switch language
    bool switch_language(const std::string& lang_id);

    // Switch UI language
    void switch_ui_language(const std::string& lang_code);

    // Get available UI languages
    static std::vector<std::pair<std::string, std::string>> available_ui_languages();

    // Toggle settings panel
    void toggle_settings();

    // Check if settings panel is visible
    bool is_settings_visible() const;

   private:
    Renderer renderer_;
    Pty pty_;
    std::unique_ptr<Screen> screen_;
    std::unique_ptr<Parser> parser_;
    std::unique_ptr<RimeIme> ime_;
    LanguageManager language_manager_;
    InputProcessor input_processor_;
    AppConfig config_;
    size_t selected_candidate_ = 0;
    bool initialized_ = false;
    bool need_render_ = false;
    ui::SettingsState settings_state_;  // Settings panel state

    void on_language_change(const LanguageConfig& lang);
    void render_candidates_bar();
    void on_settings_change(const std::string& key, const std::string& value);
    void on_settings_close();
    void render_settings_panel();
};