#pragma once

#include <functional>
#include <vector>
#include <unordered_map>
#include <poll.h>
#include <cstdint>

// Event types
enum class EventType {
    ReadReady,
    WriteReady,
    Signal,
    Timer
};

// Event source identifier
using EventId = uint64_t;

// Event callback
template<typename... Args>
using EventCallback = std::function<void(Args...)>;

// Event data
struct Event {
    EventType type;
    int fd;
    uint32_t events;
    void* data;
};

// Event loop
class EventLoop {
public:
    EventLoop();
    ~EventLoop();

    // Run the event loop
    void run();

    // Stop the event loop
    void stop();

    // Register a file descriptor for read events
    EventId add_reader(int fd, EventCallback<int> callback);

    // Register a file descriptor for write events
    EventId add_writer(int fd, EventCallback<int> callback);

    // Register a signal handler
    EventId add_signal(int signum, EventCallback<int> callback);

    // Remove an event source
    void remove(EventId id);

    // Post a callback to be executed in the next iteration
    void post(std::function<void()> callback);

private:
    bool running_ = false;
    std::vector<struct pollfd> pollfds_;
    std::unordered_map<int, EventCallback<int>> read_callbacks_;
    std::unordered_map<int, EventCallback<int>> write_callbacks_;
    std::unordered_map<int, int> signal_fds_;  // signum -> fd
    std::vector<std::function<void()>> pending_;
    EventId next_id_ = 1;
};
