#include "core/event_loop.hpp"
#include "core/app.hpp"
#include <unistd.h>
#include <signal.h>
#include <cstdio>
#include <cstdlib>

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    const char* shell = getenv("SHELL");
    if (!shell) shell = "/bin/bash";

    // Create event loop
    EventLoop loop;

    // Create application
    App app;
    if (!app.init(shell)) {
        return 1;
    }

    // Register PTY reader
    loop.watch_fd(app.pty_fd(), [&app](const char* data, size_t len) {
        app.on_pty_data(data, len);
    });

    // Register keyboard reader
    loop.watch_fd(STDIN_FILENO, [&app](const char* data, size_t len) {
        app.on_keyboard_data(data, len);
    });

    // Register signal handlers
    loop.watch_signal(SIGWINCH, [&app](int signum) {
        app.on_resize(signum);
    });

    loop.watch_signal(SIGINT, [&app, &loop](int signum) {
        app.on_quit(signum);
        loop.stop();
    });

    loop.watch_signal(SIGTERM, [&app, &loop](int signum) {
        app.on_quit(signum);
        loop.stop();
    });

    // Run event loop
    loop.run();

    return 0;
}