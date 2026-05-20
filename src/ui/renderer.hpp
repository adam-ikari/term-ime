#pragma once

#include "../terminal/screen.hpp"
#include "../ime/engine.hpp"
#include "components.hpp"
#include <string>

class Renderer {
public:
    Renderer();
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    void init();
    void restore();

    void render(const Screen& screen);
    void forward_output(const char* data, size_t len);
    void render_candidates(const std::vector<Candidate>& candidates,
                           size_t selected, const std::string& buffer,
                           const std::string& mode = "EN");

    // Extended render with AI status
    void render_candidates_ex(const std::vector<Candidate>& candidates,
                              size_t selected, const std::string& buffer,
                              const std::string& mode,
                              bool ai_enabled, bool ai_loading,
                              bool downloading, int download_progress);

    int read_key();
    int get_tty_fd() const;

private:
    int tty_fd_ = -1;
    bool initialized_ = false;
    struct termios* saved_termios_ = nullptr;

    void render_element(const ui::Element& element);
};