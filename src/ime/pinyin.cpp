#include "pinyin.hpp"
#include <cctype>

PinyinIme::PinyinIme(std::unique_ptr<Dict> dict)
    : dict_(std::move(dict)) {}

bool PinyinIme::input(char ch) {
    if (std::islower(static_cast<unsigned char>(ch))) {
        buffer_ += ch;
        state_ = ImeState::Composing;
        update_candidates();
        return true;
    }
    return false;
}

ImeState PinyinIme::state() const {
    return state_;
}

std::string PinyinIme::buffer() const {
    return buffer_;
}

std::vector<Candidate> PinyinIme::candidates() const {
    return candidates_;
}

std::u32string PinyinIme::select(int index) {
    if (index >= 0 && static_cast<size_t>(index) < candidates_.size()) {
        std::u32string result = candidates_[index].text;
        cancel();
        return result;
    }
    return {};
}

void PinyinIme::cancel() {
    buffer_.clear();
    candidates_.clear();
    page_start_ = 0;
    state_ = ImeState::Inactive;
}

void PinyinIme::page_up() {
    if (page_start_ >= page_size_) {
        page_start_ -= page_size_;
    }
}

void PinyinIme::page_down() {
    if (page_start_ + page_size_ < candidates_.size()) {
        page_start_ += page_size_;
    }
}

void PinyinIme::update_candidates() {
    candidates_.clear();
    auto words = dict_->query(buffer_);
    for (const auto& word : words) {
        candidates_.push_back({word, buffer_});
    }
}
