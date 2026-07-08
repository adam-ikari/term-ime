#include "input_processor.hpp"

InputProcessor::InputProcessor() : sm_() {}

input_sm::Result InputProcessor::process(uint8_t byte) {
    input_sm::Result result;
    input_sm::Byte event{byte, &result, &buffer_};

    sm_.process_event(event);
    return result;
}

bool InputProcessor::in_escape() const {
    using namespace input_sm;
    return sm_.is(boost::sml::state<Escape>) || sm_.is(boost::sml::state<EscapeCSI>);
}

void InputProcessor::reset() {
    sm_ = boost::sml::sm<input_sm::Machine>{};
    buffer_.clear();
}