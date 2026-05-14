#include "event_loop.hpp"
#include <spdlog/spdlog.h>
#include <unistd.h>
#include <cstring>

EventLoop::EventLoop() {
    uv_loop_init(&loop_);
    uv_async_init(&loop_, &async_handle_, async_callback);
    async_handle_.data = this;
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
    uv_close(reinterpret_cast<uv_handle_t*>(&async_handle_), nullptr);

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
    if (!running_) return;
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

void EventLoop::schedule(std::function<void()> callback) {
    {
        std::lock_guard<std::mutex> lock(pending_mutex_);
        pending_callbacks_.push_back(std::move(callback));
    }
    uv_async_send(&async_handle_);
}

void EventLoop::queue_work(std::function<void()> work, std::function<void()> after) {
    auto req = new WorkHandle();
    req->work_fn = std::move(work);
    req->after_fn = std::move(after);
    req->work.data = req;

    uv_queue_work(&loop_, &req->work, work_callback, after_work_callback);
}

// Static callbacks
void EventLoop::timer_callback(uv_timer_t* handle) {
    auto* timer = static_cast<TimerHandle*>(handle->data);
    if (timer && timer->callback) {
        timer->callback();
    }
}

void EventLoop::io_callback(uv_poll_t* handle, int status, int events) {
    auto* io = static_cast<IoHandle*>(handle->data);
    if (io && io->callback && status == 0) {
        // Read available data
        char buf[4096];
        ssize_t n = read(io->fd, buf, sizeof(buf));
        if (n > 0) {
            io->callback(buf, n);
        }
    }
}

void EventLoop::signal_callback(uv_signal_t* handle, int signum) {
    auto* sig = static_cast<SignalHandle*>(handle->data);
    if (sig && sig->callback) {
        sig->callback(signum);
    }
}

void EventLoop::work_callback(uv_work_t* req) {
    auto* work = static_cast<WorkHandle*>(req->data);
    if (work && work->work_fn) {
        work->work_fn();
    }
}

void EventLoop::after_work_callback(uv_work_t* req, int status) {
    auto* work = static_cast<WorkHandle*>(req->data);
    if (work) {
        if (work->after_fn) {
            work->after_fn();
        }
        delete work;
    }
}

void EventLoop::async_callback(uv_async_t* handle) {
    auto* loop = static_cast<EventLoop*>(handle->data);
    if (!loop) return;

    std::vector<std::function<void()>> callbacks;
    {
        std::lock_guard<std::mutex> lock(loop->pending_mutex_);
        callbacks = std::move(loop->pending_callbacks_);
        loop->pending_callbacks_.clear();
    }

    for (auto& cb : callbacks) {
        cb();
    }
}