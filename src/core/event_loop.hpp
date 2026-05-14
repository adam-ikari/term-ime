#pragma once

#include <uv.h>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>

// Libuv-based event loop for async I/O
class EventLoop {
public:
    using TimerCallback = std::function<void()>;
    using IoCallback = std::function<void(const char* data, size_t len)>;
    using SignalCallback = std::function<void(int signum)>;

    EventLoop();
    ~EventLoop();

    // Non-copyable
    EventLoop(const EventLoop&) = delete;
    EventLoop& operator=(const EventLoop&) = delete;

    // Get the underlying uv_loop
    uv_loop_t* loop() { return &loop_; }

    // Run the event loop
    void run();

    // Stop the event loop
    void stop();

    // Check if running
    bool is_running() const { return running_; }

    // Timer management
    uint64_t set_timer(TimerCallback callback, uint64_t timeout_ms, bool repeat = false);
    void clear_timer(uint64_t timer_id);

    // Async I/O
    void watch_fd(int fd, IoCallback callback, bool readable = true);
    void unwatch_fd(int fd);

    // Signal handling
    void watch_signal(int signum, SignalCallback callback);
    void unwatch_signal(int signum);

    // Schedule callback on next tick
    void schedule(std::function<void()> callback);

    // Async work (run in thread pool, then callback on main thread)
    void queue_work(std::function<void()> work, std::function<void()> after);

private:
    uv_loop_t loop_;
    bool running_ = false;

    // Internal structures for tracking handles
    struct TimerHandle {
        uv_timer_t handle;
        TimerCallback callback;
        uint64_t id;
    };

    struct IoHandle {
        uv_poll_t handle;
        IoCallback callback;
        int fd;
    };

    struct SignalHandle {
        uv_signal_t handle;
        SignalCallback callback;
        int signum;
    };

    struct WorkHandle {
        uv_work_t work;
        std::function<void()> work_fn;
        std::function<void()> after_fn;
    };

    std::unordered_map<uint64_t, std::unique_ptr<TimerHandle>> timers_;
    std::unordered_map<int, std::unique_ptr<IoHandle>> io_handles_;
    std::unordered_map<int, std::unique_ptr<SignalHandle>> signal_handles_;
    uint64_t next_timer_id_ = 1;

    static void timer_callback(uv_timer_t* handle);
    static void io_callback(uv_poll_t* handle, int status, int events);
    static void signal_callback(uv_signal_t* handle, int signum);
    static void work_callback(uv_work_t* req);
    static void after_work_callback(uv_work_t* req, int status);
    static void async_callback(uv_async_t* handle);

    uv_async_t async_handle_;
    std::vector<std::function<void()>> pending_callbacks_;
    std::mutex pending_mutex_;
};