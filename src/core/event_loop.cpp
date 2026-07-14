#include "event_loop.hpp"
#include <spdlog/spdlog.h>
#include <unistd.h>
#include <cstring>

EventLoop::EventLoop() {
    uv_loop_init(&loop_);
}

EventLoop::~EventLoop() {
    stop();

    // Close all handles
    for (auto& [id, timer] : timers_) {
        uv_close(reinterpret_cast<uv_handle_t*>(&timer->handle), nullptr);
    }
    for (auto& [fd, io] : io_handles_) {
        uv_close(reinterpret_cast<uv_handle_t*>(&io->handle), nullptr);
    }
    for (auto& [signum, sig] : signal_handles_) {
        uv_close(reinterpret_cast<uv_handle_t*>(&sig->handle), nullptr);
    }

    // Run loop once to process closes
    uv_run(&loop_, UV_RUN_NOWAIT);
    uv_loop_close(&loop_);
}

void EventLoop::run() {
    running_ = true;
    spdlog::debug("EventLoop started");
    uv_run(&loop_, UV_RUN_DEFAULT);
    running_ = false;
    spdlog::debug("EventLoop stopped");
}

void EventLoop::stop() {
    if (!running_)
        return;
    running_ = false;
    uv_stop(&loop_);
}

uint64_t EventLoop::set_timer(TimerCallback callback, uint64_t timeout_ms, bool repeat) {
    auto handle = std::make_unique<TimerHandle>();
    handle->callback = std::move(callback);
    handle->id = next_timer_id_++;
    handle->handle.data = handle.get();

    uv_timer_init(&loop_, &handle->handle);
    uv_timer_start(&handle->handle, timer_callback, timeout_ms, repeat ? timeout_ms : 0);

    uint64_t id = handle->id;
    timers_[id] = std::move(handle);
    return id;
}

void EventLoop::clear_timer(uint64_t timer_id) {
    auto it = timers_.find(timer_id);
    if (it != timers_.end()) {
        uv_close(reinterpret_cast<uv_handle_t*>(&it->second->handle), nullptr);
        timers_.erase(it);
    }
}

void EventLoop::watch_fd(int fd, IoCallback callback, bool readable) {
    auto handle = std::make_unique<IoHandle>();
    handle->callback = std::move(callback);
    handle->fd = fd;
    handle->handle.data = handle.get();

    int events = readable ? UV_READABLE : UV_WRITABLE;
    uv_poll_init(&loop_, &handle->handle, fd);
    uv_poll_start(&handle->handle, events, io_callback);

    io_handles_[fd] = std::move(handle);
}

void EventLoop::unwatch_fd(int fd) {
    auto it = io_handles_.find(fd);
    if (it != io_handles_.end()) {
        uv_close(reinterpret_cast<uv_handle_t*>(&it->second->handle), nullptr);
        io_handles_.erase(it);
    }
}

void EventLoop::watch_signal(int signum, SignalCallback callback) {
    auto handle = std::make_unique<SignalHandle>();
    handle->callback = std::move(callback);
    handle->signum = signum;
    handle->handle.data = handle.get();

    uv_signal_init(&loop_, &handle->handle);
    uv_signal_start(&handle->handle, signal_callback, signum);

    signal_handles_[signum] = std::move(handle);
}

void EventLoop::unwatch_signal(int signum) {
    auto it = signal_handles_.find(signum);
    if (it != signal_handles_.end()) {
        uv_close(reinterpret_cast<uv_handle_t*>(&it->second->handle), nullptr);
        signal_handles_.erase(it);
    }
}

// Static callbacks
void EventLoop::timer_callback(uv_timer_t* handle) {
    auto* timer = static_cast<TimerHandle*>(handle->data);
    if (timer && timer->callback) {
        timer->callback();
    }
}

void EventLoop::io_callback(uv_poll_t* handle, int /*status*/, int /*events*/) {
    auto* io = static_cast<IoHandle*>(handle->data);
    if (!io || !io->callback)
        return;

    // Read available data
    char buf[4096];
    ssize_t n = read(io->fd, buf, sizeof(buf));

    if (n > 0) {
        io->callback(buf, n);
    } else if (n == 0) {
        // EOF - PTY closed, shell exited
        spdlog::info("EOF on fd {}, calling callback with len=0", io->fd);
        io->callback(nullptr, 0);
    } else {
        // n < 0: check if it's a real error or just EAGAIN
        if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
            spdlog::warn("Read error on fd {}: {}", io->fd, strerror(errno));
            io->callback(nullptr, 0);
        }
    }
}

void EventLoop::signal_callback(uv_signal_t* handle, int signum) {
    auto* sig = static_cast<SignalHandle*>(handle->data);
    if (sig && sig->callback) {
        sig->callback(signum);
    }
}