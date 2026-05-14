#include "terminal/pty.hpp"
#include "terminal/screen.hpp"
#include "terminal/parser.hpp"
#include "ime/pinyin.hpp"
#include "ime/dict.hpp"
#include "ui/renderer.hpp"
#include "util/utf8.hpp"

#include <unistd.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <cstdio>
#include <cstdlib>
#include <cerrno>

static volatile bool running = true;
static volatile bool need_resize = false;
static Screen* g_screen = nullptr;
static Pty* g_pty = nullptr;
static Renderer* g_renderer = nullptr;

void signal_handler(int sig) {
    if (sig == SIGWINCH) {
        need_resize = true;
    } else {
        running = false;
    }
}

void handle_resize() {
    if (!g_screen || !g_pty || !g_renderer) return;

    struct winsize ws;
    ioctl(g_renderer->get_tty_fd(), TIOCGWINSZ, &ws);

    g_screen->resize(ws.ws_row - 1, ws.ws_col);
    g_pty->resize(ws.ws_row - 1, ws.ws_col);

    need_resize = false;
}

int main(int argc, char* argv[]) {
    const char* shell = getenv("SHELL");
    if (!shell) shell = "/bin/bash";

    // Initialize renderer (takes over TTY)
    Renderer renderer;
    renderer.init();

    // Set up signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGWINCH, signal_handler);

    // Create PTY and spawn shell
    Pty pty;
    g_pty = &pty;
    if (!pty.spawn(shell)) {
        fprintf(stderr, "Failed to spawn shell\n");
        renderer.restore();
        return 1;
    }

    // Get terminal size
    struct winsize ws;
    if (ioctl(renderer.get_tty_fd(), TIOCGWINSZ, &ws) < 0 || ws.ws_row == 0 || ws.ws_col == 0) {
        ws.ws_row = 24;
        ws.ws_col = 80;
    }

    // Create screen and parser
    int screen_rows = std::max(1, ws.ws_row - 1);  // Reserve bottom line for IME
    int screen_cols = std::max(1, (int)ws.ws_col);
    Screen screen(screen_rows, screen_cols);
    g_screen = &screen;
    Parser parser(screen);

    // Set global renderer for resize
    g_renderer = &renderer;

    // Load IME
    auto dict = std::make_unique<Dict>();
    if (!dict->load("data/pinyin.dict")) {
        fprintf(stderr, "Warning: Failed to load dictionary\n");
    } else {
        fprintf(stderr, "Loaded dictionary: %zu entries, %zu pinyin\n",
                dict->entry_count(), dict->pinyin_count());
    }
    PinyinIme ime(std::move(dict));

    std::string ime_buffer;
    size_t selected_candidate = 0;

    // Main loop
    while (running) {
        // Handle resize signal
        if (need_resize) {
            handle_resize();
            renderer.render(screen);
        }

        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(pty.fd(), &fds);
        FD_SET(STDIN_FILENO, &fds);

        int maxfd = std::max(pty.fd(), STDIN_FILENO);

        struct timeval tv = {0, 10000};  // 10ms timeout
        int ret = select(maxfd + 1, &fds, nullptr, nullptr, &tv);

        if (ret < 0) {
            if (errno == EINTR) continue;
            break;
        }

        // Read from PTY (shell output)
        if (FD_ISSET(pty.fd(), &fds)) {
            auto data = pty.read();
            if (data) {
                parser.feed(data->data(), data->size());
                renderer.render(screen);

                // Re-render candidates if IME is active
                if (ime.state() != ImeState::Inactive) {
                    renderer.render_candidates(ime.candidates(), selected_candidate, ime.buffer());
                }
            }
        }

        // Read from keyboard
        if (FD_ISSET(STDIN_FILENO, &fds)) {
            int ch = renderer.read_key();
            if (ch < 0) continue;

            // Check if IME should handle this
            if (ime.state() == ImeState::Composing || ime.state() == ImeState::Selecting) {
                if (ch >= '1' && ch <= '9') {
                    // Select candidate
                    int idx = ch - '1';
                    auto result = ime.select(idx);
                    if (!result.empty()) {
                        // Send UTF-8 to PTY
                        std::string utf8 = utf8::encode(result[0]);
                        for (size_t i = 1; i < result.size(); ++i) {
                            utf8 += utf8::encode(result[i]);
                        }
                        pty.write(std::vector<uint8_t>(utf8.begin(), utf8.end()));
                    }
                    renderer.render(screen);
                } else if (ch == 27) {  // Escape
                    ime.cancel();
                    renderer.render(screen);
                } else if (ch == '\b' || ch == 127) {  // Backspace
                    ime.cancel();
                    renderer.render(screen);
                } else if (ch >= 'a' && ch <= 'z') {
                    ime.input(static_cast<char>(ch));
                    selected_candidate = 0;
                    renderer.render(screen);
                    renderer.render_candidates(ime.candidates(), selected_candidate, ime.buffer());
                } else {
                    // Pass through to PTY
                    char c = static_cast<char>(ch);
                    pty.write(std::vector<uint8_t>(c, c + 1));
                }
            } else {
                // Normal mode
                if (ch >= 'a' && ch <= 'z') {
                    // Start IME input
                    ime.input(static_cast<char>(ch));
                    selected_candidate = 0;
                    renderer.render(screen);
                    renderer.render_candidates(ime.candidates(), selected_candidate, ime.buffer());
                } else {
                    // Pass through to PTY
                    char c = static_cast<char>(ch);
                    pty.write(std::vector<uint8_t>(&c, &c + 1));
                }
            }
        }
    }

    renderer.restore();
    return 0;
}
