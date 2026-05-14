#include "core/event_loop.hpp"
#include "core/app.hpp"
#include <unistd.h>
#include <signal.h>
#include <cstdio>
#include <cstdlib>

int main(int argc, char* argv[]) {
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
    loop.add_reader(app.pty_fd(), [&app](int fd) {
        app.on_pty_output(fd);
    });

    // Register keyboard reader
    loop.add_reader(STDIN_FILENO, [&app](int fd) {
        app.on_keyboard_input(fd);
    });

    // Register signal handlers
    loop.add_signal(SIGWINCH, [&app](int signum) {
        app.on_resize(signum);
    });

    loop.add_signal(SIGINT, [&app, &loop](int signum) {
        app.on_quit(signum);
        loop.stop();
    });

    loop.add_signal(SIGTERM, [&app, &loop](int signum) {
        app.on_quit(signum);
        loop.stop();
    });

    // Run event loop
    loop.run();

    return 0;
}