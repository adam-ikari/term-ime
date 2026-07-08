#pragma once

#include "../terminal/screen.hpp"
#include "../ime/engine.hpp"
#include "components.hpp"
#include "settings.hpp"
#include <string>

class Renderer {
   public:
    Renderer();
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    void init();
    void restore();

    // Re-issue the scroll region (excludes the status-bar row). Call on resize.
    void update_scroll_region();

    void render(const Screen& screen);
    void forward_output(const char* data, size_t len);
    void render_candidates(const std::vector<Candidate>& candidates, size_t selected, const std::string& buffer,
                           const std::string& mode = "EN");

    // Extended render with AI status
    void render_candidates_ex(const std::vector<Candidate>& candidates, size_t selected, const std::string& buffer,
                              const std::string& mode, bool ai_enabled, bool ai_loading, bool downloading,
                              int download_progress);

    // Render settings panel
    void render_settings(ui::SettingsState& state);

    int read_key();
    int get_tty_fd() const;

   private:
    int tty_fd_ = -1;
    bool initialized_ = false;
    struct termios* saved_termios_ = nullptr;

    // Signature of the last-rendered status bar (mode + ai flags + progress +
    // candidate count + buffer). When the IME is inactive this stays stable,
    // so the repeated render_candidates_bar() calls driven by shell output can
    // be skipped as a no-op (monkey finding F4). Empty means "force next draw".
    // A skip counter bounds how long we go without repainting, so a shell
    // `clear` (ESC[2J wipes the status bar) is recovered within ~16 outputs.
    std::string last_bar_sig_;
    int bar_skip_count_ = 0;
    static constexpr int BAR_FORCE_REDRAW_EVERY = 16;

    void render_element(const ui::Element& element);
};