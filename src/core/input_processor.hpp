#pragma once

#include <boost/sml.hpp>
#include <vector>
#include <cstdint>
#include <spdlog/spdlog.h>

namespace input_sm {

namespace sml = boost::sml;

// State tags
struct Normal {};
struct Escape {};
struct EscapeCSI {};
struct Prefix {};

// Output result from processing
struct Result {
    std::vector<uint8_t> data;
    bool forward = false;
    bool toggle_mode = false;
};

// Events
struct Byte {
    uint8_t value;
    Result* result;
    std::vector<uint8_t>* buffer;
};

// State machine definition
struct Machine {
    auto operator()() const noexcept {
        using namespace sml;

        // Guards
        const auto is_ctrl_a = [](const Byte& e) { return e.value == 1; };
        const auto is_esc = [](const Byte& e) { return e.value == 0x1b; };
        const auto is_csi = [](const Byte& e) { return e.value == '[' || e.value == 'O'; };
        const auto is_terminator = [](const Byte& e) {
            return (e.value >= 'A' && e.value <= 'Z') || (e.value >= 'a' && e.value <= 'z') || e.value == '~';
        };
        const auto is_space = [](const Byte& e) { return e.value == ' '; };

        // Actions
        const auto forward_byte = [](const Byte& e) {
            e.result->data = {e.value};
            e.result->forward = true;
        };

        const auto start_escape = [](const Byte& e) {
            e.buffer->clear();
            e.buffer->push_back(0x1b);
            spdlog::debug("SM: Normal -> Escape");
        };

        const auto buffer_byte = [](const Byte& e) {
            // Cap the escape-sequence buffer: a well-formed CSI is at most a
            // few dozen bytes. A flood of non-terminator bytes (e.g. a monkey
            // feeding ESC[ + thousands of digits) would otherwise grow this
            // without bound (monkey finding F4). Drop the front when it gets
            // oversized rather than letting memory balloon.
            if (e.buffer->size() > 256) {
                e.buffer->clear();
                e.buffer->push_back(0x1b);
                e.buffer->push_back('[');
            }
            e.buffer->push_back(e.value);
        };

        const auto forward_escape = [](const Byte& e) {
            e.result->data = *e.buffer;
            e.result->data.push_back(e.value);
            e.result->forward = true;
            spdlog::debug("SM: Escape -> Normal (forward)");
        };

        const auto complete_escape = [](const Byte& e) {
            e.buffer->push_back(e.value);
            e.result->data = *e.buffer;
            e.result->forward = true;
            spdlog::debug("SM: EscapeCSI -> Normal (complete)");
        };

        const auto toggle_mode = [](const Byte& e) {
            e.result->toggle_mode = true;
            spdlog::debug("SM: Prefix -> Normal (toggle)");
        };

        const auto forward_prefix = [](const Byte& e) {
            e.result->data = {1, e.value};
            e.result->forward = true;
            spdlog::debug("SM: Prefix -> Normal (forward)");
        };

        const auto forward_literal_ctrl_a = [](const Byte& e) {
            e.result->data = {1};
            e.result->forward = true;
            spdlog::debug("SM: Prefix -> Normal (literal Ctrl+A)");
        };

        return make_transition_table(
            // Normal state
            *state<Normal> + event<Byte>[is_ctrl_a] = state<Prefix>,
            state<Normal> + event<Byte>[is_esc] / start_escape = state<Escape>,
            state<Normal> + event<Byte> / forward_byte,

            // Escape state
            state<Escape> + event<Byte>[is_csi] / buffer_byte = state<EscapeCSI>,
            state<Escape> + event<Byte> / forward_escape = state<Normal>,

            // EscapeCSI state
            state<EscapeCSI> + event<Byte>[is_terminator] / complete_escape = state<Normal>,
            state<EscapeCSI> + event<Byte> / buffer_byte,

            // Prefix state
            state<Prefix> + event<Byte>[is_space] / toggle_mode = state<Normal>,
            state<Prefix> + event<Byte>[is_ctrl_a] / forward_literal_ctrl_a = state<Normal>,
            state<Prefix> + event<Byte> / forward_prefix = state<Normal>);
    }
};

}  // namespace input_sm

// Input processor using Boost.SML state machine
class InputProcessor {
   public:
    InputProcessor();

    // Process a byte, return result
    input_sm::Result process(uint8_t byte);

    // Check if currently in escape sequence
    bool in_escape() const;

    // Reset state
    void reset();

   private:
    boost::sml::sm<input_sm::Machine> sm_;
    std::vector<uint8_t> buffer_;
};