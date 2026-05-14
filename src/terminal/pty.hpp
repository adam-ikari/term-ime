#pragma once

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
    bool write(const std::vector<uint8_t>& data);
    int fd() const;
    void resize(int rows, int cols);

private:
    int master_fd_ = -1;
    int pid_ = -1;
};
