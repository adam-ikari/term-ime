#pragma once

#include "engine.hpp"
#include <rime_api.h>
#include <string>
#include <vector>
#include <memory>

// Rime-based input method engine wrapper
class RimeIme : public ImeEngine {
public:
    // Initialize with optional data directories
    // shared_data_dir: system rime data directory (default: /usr/share/rime-data)
    // user_data_dir: user config directory (default: ~/.config/term-ime)
    explicit RimeIme(const std::string& shared_data_dir = "",
                     const std::string& user_data_dir = "");
    ~RimeIme();

    bool input(char ch) override;
    ImeState state() const override;
    ImeMode mode() const override;
    void set_mode(ImeMode mode) override;
    void toggle_mode() override;
    std::string buffer() const override;
    std::vector<Candidate> candidates() const override;
    std::u32string select(int index) override;
    void cancel() override;
    void page_up() override;
    void page_down() override;

    // Rime-specific methods
    bool select_schema(const std::string& schema_id);
    std::vector<std::string> get_schema_list();
    std::string get_current_schema();

    // Initialize rime engine
    bool initialize();

private:
    RimeApi* rime_ = nullptr;
    RimeSessionId session_ = 0;
    ImeMode mode_ = ImeMode::English;  // 默认英文模式，不影响终端正常使用
    std::string shared_data_dir_;
    std::string user_data_dir_;

    void update_state();
    std::u32string utf8_to_utf32(const std::string& utf8) const;
};