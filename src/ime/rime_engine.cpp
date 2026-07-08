#include "rime_engine.hpp"
#include "../util/utf8.hpp"
#include <cstring>
#include <filesystem>

RimeIme::RimeIme(const std::string& shared_data_dir, const std::string& user_data_dir)
    : shared_data_dir_(shared_data_dir), user_data_dir_(user_data_dir) {}

RimeIme::~RimeIme() {
    if (rime_ && session_) {
        rime_->destroy_session(session_);
        rime_->finalize();
    }
}

bool RimeIme::initialize() {
    rime_ = rime_get_api();
    if (!rime_) {
        return false;
    }

    // Setup traits
    RIME_STRUCT(RimeTraits, traits);

    // Determine shared data directory
    std::string shared_dir = shared_data_dir_;
    if (shared_dir.empty()) {
        // Try common locations
        std::vector<std::string> search_paths = {"/usr/share/rime-data", "/usr/local/share/rime-data",
                                                 "/opt/rime-data"};

        for (const auto& path : search_paths) {
            if (std::filesystem::exists(path)) {
                shared_dir = path;
                break;
            }
        }

        if (shared_dir.empty()) {
            shared_dir = "/usr/share/rime-data";  // fallback
        }
    }
    traits.shared_data_dir = shared_dir.c_str();

    // Use default user data dir if not specified
    std::string user_dir = user_data_dir_;
    if (user_dir.empty()) {
        const char* home = getenv("HOME");
        if (home) {
            user_dir = std::filesystem::path(home) / ".config" / "term-ime";
        } else {
            user_dir = "/tmp/term-ime";
        }
    }
    traits.user_data_dir = user_dir.c_str();

    traits.distribution_name = "term-ime";
    traits.distribution_code_name = "term-ime";
    traits.distribution_version = "1.0.0";
    traits.app_name = "rime.term-ime";

    rime_->setup(&traits);
    rime_->initialize(nullptr);

    // Run maintenance if needed
    if (rime_->start_maintenance(False)) {
        rime_->join_maintenance_thread();
    }

    // Create session
    session_ = rime_->create_session();
    return session_ != 0;
}

bool RimeIme::input(char ch) {
    if (mode_ == ImeMode::English) {
        return false;
    }

    if (!rime_ || !session_)
        return false;

    // Process key
    if (rime_->process_key(session_, ch, 0)) {
        update_state();
        return true;
    }
    return false;
}

ImeState RimeIme::state() const {
    if (!rime_ || !session_)
        return ImeState::Inactive;

    RIME_STRUCT(RimeContext, context);
    if (rime_->get_context(session_, &context)) {
        bool has_composition = context.composition.length > 0;
        bool has_candidates = context.menu.num_candidates > 0;
        rime_->free_context(&context);

        if (has_candidates) {
            return ImeState::Selecting;
        }
        if (has_composition) {
            return ImeState::Composing;
        }
    }
    return ImeState::Inactive;
}

ImeMode RimeIme::mode() const {
    return mode_;
}

void RimeIme::set_mode(ImeMode mode) {
    mode_ = mode;
}

void RimeIme::toggle_mode() {
    mode_ = (mode_ == ImeMode::Chinese) ? ImeMode::English : ImeMode::Chinese;
}

std::string RimeIme::buffer() const {
    if (!rime_ || !session_)
        return "";

    RIME_STRUCT(RimeContext, context);
    if (rime_->get_context(session_, &context)) {
        const char* preedit = context.composition.preedit;
        std::string result(preedit ? preedit : "");
        rime_->free_context(&context);
        return result;
    }
    return "";
}

std::vector<Candidate> RimeIme::candidates() const {
    std::vector<Candidate> result;
    if (!rime_ || !session_)
        return result;

    RIME_STRUCT(RimeContext, context);
    if (rime_->get_context(session_, &context)) {
        for (int i = 0; i < context.menu.num_candidates; ++i) {
            Candidate cand;
            const char* text = context.menu.candidates[i].text;
            const char* comment = context.menu.candidates[i].comment;

            // Safety check for null pointers
            if (text) {
                cand.text = utf8_to_utf32(text);
            }
            cand.code = comment ? comment : "";
            result.push_back(cand);
        }
        rime_->free_context(&context);
    }
    return result;
}

std::u32string RimeIme::select(int index) {
    if (!rime_ || !session_)
        return U"";

    // Select candidate by number
    char key = '1' + index;
    rime_->process_key(session_, key, 0);

    // Get committed text
    RIME_STRUCT(RimeCommit, commit);
    if (rime_->get_commit(session_, &commit)) {
        const char* text = commit.text;
        std::u32string result = text ? utf8_to_utf32(text) : U"";
        rime_->free_commit(&commit);
        return result;
    }
    return U"";
}

void RimeIme::cancel() {
    if (!rime_ || !session_)
        return;

    // Send Escape to cancel
    rime_->process_key(session_, 0xFF1B, 0);  // XK_Escape
    update_state();
}

void RimeIme::page_up() {
    if (!rime_ || !session_)
        return;
    rime_->process_key(session_, 0xFF55, 0);  // XK_Page_Up
}

void RimeIme::page_down() {
    if (!rime_ || !session_)
        return;
    rime_->process_key(session_, 0xFF56, 0);  // XK_Page_Down
}

bool RimeIme::select_schema(const std::string& schema_id) {
    if (!rime_ || !session_)
        return false;
    return rime_->select_schema(session_, schema_id.c_str());
}

std::vector<std::string> RimeIme::get_schema_list() {
    std::vector<std::string> result;
    if (!rime_)
        return result;

    RimeSchemaList list;
    if (rime_->get_schema_list(&list)) {
        for (size_t i = 0; i < list.size; ++i) {
            result.push_back(list.list[i].schema_id);
        }
        rime_->free_schema_list(&list);
    }
    return result;
}

std::string RimeIme::get_current_schema() {
    if (!rime_ || !session_)
        return "";

    char schema[64];
    if (rime_->get_current_schema(session_, schema, sizeof(schema))) {
        return schema;
    }
    return "";
}

void RimeIme::update_state() {
    // State is updated automatically by librime
}

std::u32string RimeIme::utf8_to_utf32(const std::string& utf8) const {
    std::u32string result;
    if (utf8.empty())
        return result;

    const uint8_t* data = reinterpret_cast<const uint8_t*>(utf8.data());
    size_t pos = 0;
    while (pos < utf8.size()) {
        char32_t ch = utf8::decode(data, utf8.size(), pos);
        if (ch != 0) {  // Skip null characters
            result += ch;
        }
    }
    return result;
}