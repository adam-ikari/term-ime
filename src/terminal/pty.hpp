#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <optional>

class Pty {
   public:
    Pty();
    ~Pty();

    Pty(const Pty&) = delete;
    Pty& operator=(const Pty&) = delete;

    bool spawn(const std::string& shell = "/bin/bash");
    std::optional<std::vector<uint8_t>> read();
    // Hand `data` to the slave. Bytes a previous call could not send are queued
    // and go out first, so a stalled slave delays the stream instead of dropping
    // committed text. Returns false when the tail is still queued.
    bool write(const std::vector<uint8_t>& data);
    // Retry the queued tail (call from the event loop when the slave has
    // demonstrably made progress). True once nothing is left pending.
    bool flush();
    int fd() const;
    void resize(int rows, int cols);

   private:
    int master_fd_ = -1;
    int pid_ = -1;
    std::vector<uint8_t> tx_pending_;

    // Poll waitpid(WNOHANG) until the child exits or timeout_ms elapses.
    // Returns true if the child was reaped within the timeout.
    bool wait_for_exit(int timeout_ms);
    // Send the whole buffer or queue whatever the kernel did not take.
    bool write_raw(const uint8_t* data, size_t len);
};
