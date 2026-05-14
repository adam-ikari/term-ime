#pragma once

#include <string>
#include <vector>

enum class ImeState {
    Inactive,
    Composing,
    Selecting
};

struct Candidate {
    std::u32string text;
    std::string code;
};

class ImeEngine {
public:
    virtual ~ImeEngine() = default;

    virtual bool input(char ch) = 0;
    virtual ImeState state() const = 0;
    virtual std::string buffer() const = 0;
    virtual std::vector<Candidate> candidates() const = 0;
    virtual std::u32string select(int index) = 0;
    virtual void cancel() = 0;
    virtual void page_up() = 0;
    virtual void page_down() = 0;
};
