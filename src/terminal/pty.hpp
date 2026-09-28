#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <optional>

// Outbound byte queue for the pty master. Two rules define it:
//   - It is ordered: queued bytes are older than anything appended later, and
//     they go out ahead of it.
//   - On overflow only the NEWEST bytes are discarded. Erasing from the front
//     would splice the stream in the middle of a UTF-8 sequence or an escape
//     sequence, and the child's parser would chase that garbage for the rest of
//     the session.
class TxByteQueue {
   public:
    static constexpr size_t kMaxBytes = 64 * 1024;

    // Buffer the unsent tail: bytes [from, len) of `data`. This replaces the
    // queue, which is correct only because `data` already begins with the bytes
    // it held (write() puts them ahead of the new ones). Returns how many
    // newest bytes had to be dropped to stay within kMaxBytes.
    size_t buffer_tail(const uint8_t* data, size_t len, size_t from) {
        bytes_.assign(data + from, data + len);
        if (bytes_.size() <= kMaxBytes)
            return 0;
        const size_t dropped = bytes_.size() - kMaxBytes;
        bytes_.resize(kMaxBytes);
        return dropped;
    }

    bool empty() const { return bytes_.empty(); }
    size_t size() const { return bytes_.size(); }
    // Take the queued bytes for a retry attempt (the queue is left empty).
    std::vector<uint8_t> take() {
        std::vector<uint8_t> out;
        out.swap(bytes_);
        return out;
    }
    void clear() { bytes_.clear(); }

   private:
    std::vector<uint8_t> bytes_;
};

class Pty {
   public:
    Pty();
    ~Pty();

    Pty(const Pty&) = delete;
    Pty& operator=(const Pty&) = delete;

    bool spawn(const std::string& shell = "/bin/bash");
    std::optional<std::vector<uint8_t>> read();
    // Hand `data` to the slave. Bytes a previous call could not send (short
    // write or EAGAIN) are queued and go out first, so a stalled slave delays
    // the stream instead of cutting it mid-sequence. Returns false while bytes
    // remain queued. A queue that itself overflows drops its newest bytes and
    // says so at error level -- see TxByteQueue.
    bool write(const std::vector<uint8_t>& data);
    // Retry the queued tail. The default budget is a single non-blocking
    // attempt, safe on the output hot path; a positive budget blocks this
    // thread in poll() up to that long. True once nothing is left pending.
    bool flush(int budget_ms = 0);
    int fd() const;
    void resize(int rows, int cols);

   private:
    int master_fd_ = -1;
    int pid_ = -1;
    TxByteQueue tx_queue_;

    // Poll waitpid(WNOHANG) until the child exits or timeout_ms elapses.
    // Returns true if the child was reaped within the timeout.
    bool wait_for_exit(int timeout_ms);
    // Send the whole buffer or queue whatever the kernel did not take. On a
    // dead peer the queue is dropped instead: nothing can ever drain it.
    bool write_raw(const uint8_t* data, size_t len, int budget_ms);
};
