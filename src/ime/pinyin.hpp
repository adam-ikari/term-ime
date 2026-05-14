#pragma once

#include "engine.hpp"
#include "dict.hpp"
#include <memory>

class PinyinIme : public ImeEngine {
public:
    explicit PinyinIme(std::unique_ptr<Dict> dict);

    bool input(char ch) override;
    ImeState state() const override;
    std::string buffer() const override;
    std::vector<Candidate> candidates() const override;
    std::u32string select(int index) override;
    void cancel() override;
    void page_up() override;
    void page_down() override;

private:
    void update_candidates();

    std::unique_ptr<Dict> dict_;
    std::string buffer_;
    std::vector<Candidate> candidates_;
    size_t page_start_ = 0;
    size_t page_size_ = 5;
    ImeState state_ = ImeState::Inactive;
};
