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
    explicit RimeIme(const std::string& shared_data_dir = "", const std::string& user_data_dir = "");
    ~RimeIme();

    bool input(char ch) override;
    ImeState state() const override;
    ImeMode mode() const override;
    void set_mode(ImeMode mode) override;
    void toggle_mode() override;
    std::string buffer() const override;
    std::vector<Candidate> candidates() const override;
    std::u32string select(int index) override;
    void backspace() override;
    void cancel() override;
    void page_up() override;
    void page_down() override;

    // Rime-specific methods
    bool select_schema(const std::string& schema_id);
    std::vector<std::string> get_schema_list();
    std::string get_current_schema();

    // Initialize rime engine
    bool initialize();
    // Fuzzy pinyin groups (each a settings toggle):
    //   zh_z 平翘舌 | n_l | r (r/l、r/y) | hu_f | nose (前后鼻音 en/eng、an/ang)
    // All groups on → the bundled all-on fuzzy schema; a subset → a generated
    // per-combination schema (luna_pinyin_simp_fuzzy_<sig>); none → precise.
    void set_fuzzy_groups(const std::vector<std::string>& groups);
    const std::vector<std::string>& fuzzy_groups() const { return fuzzy_groups_; }
    // The fuzzy schema id for the configured groups: the bundled all-on twin,
    // a generated per-combination twin, or `schema_id` itself when precise.
    std::string fuzzy_variant(const std::string& schema_id) const;

   private:
    // Stateful deleter for rime_life_: closes the current session (if one was
    // created) and finalizes librime's global state exactly once.
    struct RimeShutdown {
        RimeIme* owner = nullptr;
        void operator()(RimeApi* api) const;
    };

    RimeApi* rime_ = nullptr;  // borrowed from rime_get_api(); never owned here
    // Non-null exactly between a successful rime_->initialize() and its
    // finalize(), so every failure path below and ~RimeIme release librime.
    std::unique_ptr<RimeApi, RimeShutdown> rime_life_;
    RimeSessionId session_ = 0;
    ImeMode mode_ = ImeMode::English;  // 默认英文模式，不影响终端正常使用
    std::string shared_data_dir_;
    std::string user_data_dir_;
    // Resolved at initialize(): rime's actual data dirs (XDG fallbacks applied).
    std::string resolved_shared_dir_;
    std::string resolved_user_dir_;
    std::vector<std::string> fuzzy_groups_ = {"zh_z", "n_l", "r", "hu_f", "nose"};

    // The per-combination schema id suffix, e.g. "zh_z_n_l" — empty when all
    // groups are on or all off (those use the bundled schemas directly).
    std::string fuzzy_signature() const;
    // Write the per-combination schema (template pruned to the enabled groups)
    // into the user data dir and deploy it when its prism is missing.
    void ensure_fuzzy_schema();

    void update_state();
    std::u32string utf8_to_utf32(const std::string& utf8) const;
};