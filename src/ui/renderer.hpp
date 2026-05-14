#pragma once

#include "../terminal/screen.hpp"
#include "../ime/engine.hpp"
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
    void render_candidates(const std::vector<Candidate>& candidates,
                           size_t selected, const std::string& buffer);

    int read_key();
    int get_tty_fd() const;

private:
    int tty_fd_ = -1;
    bool initialized_ = false;
    struct termios* saved_termios_ = nullptr;
};
