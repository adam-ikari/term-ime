#pragma once

#include "terminal/pty.hpp"
#include "terminal/screen.hpp"
#include "terminal/parser.hpp"
#include "ime/rime_engine.hpp"
#include "ui/renderer.hpp"
#include "util/utf8.hpp"

#include <memory>
#include <string>

// Application state
class App {
public:
    App();
    ~App();

    // Initialize application
    bool init(const std::string& shell);

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

private:
    Renderer renderer_;
    Pty pty_;
    Screen* screen_ = nullptr;
    Parser* parser_ = nullptr;
    RimeIme ime_;
    size_t selected_candidate_ = 0;
    bool initialized_ = false;

    // Prefix key state (tmux-style: Ctrl+B then command key)
    bool prefix_pending_ = false;  // True after Ctrl+B, waiting for command key
};