#include "event_loop.hpp"
#include <unistd.h>
#include <signal.h>
#include <sys/signalfd.h>
#include <cstring>
#include <cerrno>

EventLoop::EventLoop() = default;

EventLoop::~EventLoop() {
    // Close signal fds
    for (auto& [signum, fd] : signal_fds_) {
        close(fd);
    }
}

void EventLoop::run() {
    running_ = true;

    while (running_) {
        // Execute pending callbacks
        auto pending = std::move(pending_);
        pending_.clear();
        for (auto& cb : pending) {
            cb();
        }

        // Poll for events
        int n = poll(pollfds_.data(), pollfds_.size(), -1);

        if (n < 0) {
            if (errno == EINTR) continue;
            break;
        }

        // Process events
        for (auto& pfd : pollfds_) {
            if (pfd.revents & POLLIN) {
                auto it = read_callbacks_.find(pfd.fd);
                if (it != read_callbacks_.end()) {
                    it->second(pfd.fd);
                }
            }
            if (pfd.revents & POLLOUT) {
                auto it = write_callbacks_.find(pfd.fd);
                if (it != write_callbacks_.end()) {
                    it->second(pfd.fd);
                }
            }
        }
    }
}

void EventLoop::stop() {
    running_ = false;
}

EventId EventLoop::add_reader(int fd, EventCallback<int> callback) {
    read_callbacks_[fd] = callback;

    // Check if fd already in pollfds
    for (auto& pfd : pollfds_) {
        if (pfd.fd == fd) {
            pfd.events |= POLLIN;
            return next_id_++;
        }
    }

    // Add new pollfd
    pollfds_.push_back({fd, POLLIN, 0});
    return next_id_++;
}

EventId EventLoop::add_writer(int fd, EventCallback<int> callback) {
    write_callbacks_[fd] = callback;

    for (auto& pfd : pollfds_) {
        if (pfd.fd == fd) {
            pfd.events |= POLLOUT;
            return next_id_++;
        }
    }

    pollfds_.push_back({fd, POLLOUT, 0});
    return next_id_++;
}

EventId EventLoop::add_signal(int signum, EventCallback<int> callback) {
    // Create signalfd
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, signum);
    sigprocmask(SIG_BLOCK, &mask, nullptr);

    int fd = signalfd(-1, &mask, SFD_NONBLOCK);
    if (fd < 0) return 0;

    signal_fds_[signum] = fd;

    // Read signalfd and call callback
    add_reader(fd, [this, callback, signum](int fd) {
        struct signalfd_siginfo info;
        ssize_t n = read(fd, &info, sizeof(info));
        if (n == sizeof(info)) {
            callback(signum);
        }
    });

    return next_id_++;
}

void EventLoop::remove(EventId id) {
    // TODO: implement removal
    (void)id;
}

void EventLoop::post(std::function<void()> callback) {
    pending_.push_back(std::move(callback));
}
