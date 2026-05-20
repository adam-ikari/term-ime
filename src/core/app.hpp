#pragma once

#include "terminal/pty.hpp"
#include "terminal/screen.hpp"
#include "terminal/parser.hpp"
#include "ime/rime_engine.hpp"
#include "ime/language.hpp"
#include "ime/llama_ranker.hpp"
#include "ui/renderer.hpp"
#include "input_processor.hpp"
#include "config.hpp"
#include "util/utf8.hpp"
#include "util/model_downloader.hpp"

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

    // Handle quit signal
    void on_quit(int signum);

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

    // Toggle AI ranking
    void toggle_ai_ranking();

    // Check if AI ranking is enabled
    bool is_ai_ranking_enabled() const;

    // Switch UI language
    void switch_ui_language(const std::string& lang_code);

    // Get available UI languages
    static std::vector<std::pair<std::string, std::string>> available_ui_languages();

private:
    Renderer renderer_;
    Pty pty_;
    std::unique_ptr<Screen> screen_;
    std::unique_ptr<Parser> parser_;
    std::unique_ptr<RimeIme> ime_;
    std::unique_ptr<LlamaRanker> llama_ranker_;
    std::unique_ptr<ModelDownloader> model_downloader_;
    LanguageManager language_manager_;
    InputProcessor input_processor_;
    AppConfig config_;
    size_t selected_candidate_ = 0;
    bool initialized_ = false;
    std::atomic<bool> ai_ranking_enabled_{false};
    std::atomic<bool> ai_ranking_in_progress_{false};
    std::atomic<bool> model_downloading_{false};
    int model_download_progress_ = 0;
    std::vector<Candidate> last_candidates_;  // Cache for async ranking

    void on_language_change(const LanguageConfig& lang);
    void on_ranking_complete(std::vector<Candidate> ranked);
    void render_candidates_bar();
    void start_model_download();
    void on_model_downloaded(bool success, const std::string& path);
};