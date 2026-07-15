#include "rime_engine.hpp"
#include "../util/utf8.hpp"
#include <spdlog/spdlog.h>
#include <cstring>
#include <filesystem>

// Bundled rime-data dir (set by CMake when USE_BUNDLED_DEPS sources are
// built). Empty string when building against system rime-data.
#ifndef RIME_BUNDLED_DATA_DIR
#define RIME_BUNDLED_DATA_DIR ""
#endif

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

    // Determine shared data directory — prefer term-ime's own bundled data, then
    // fall back to system rime-data (which ships prebuilt prism/table .bin so no
    // runtime deploy is needed). The user data dir is always term-ime's own.
    std::string shared_dir = shared_data_dir_;
    if (shared_dir.empty()) {
        // Get user's home directory for fallback paths
        const char* home = getenv("HOME");
        std::string user_local = home ? std::string(home) + "/.local/share/term-ime/rime-data" : "";

        std::vector<std::string> search_paths = {
            RIME_BUNDLED_DATA_DIR,  // build-time bundled data
            user_local,              // user-local install
            "/usr/local/share/term-ime/rime-data",
            "/usr/share/term-ime/rime-data",
            "/usr/share/rime-data",
            "/usr/local/share/rime-data",
        };

        for (const auto& path : search_paths) {
            if (!path.empty() && std::filesystem::exists(path)) {
                shared_dir = path;
                break;
            }
        }

        if (shared_dir.empty()) {
            shared_dir = "/usr/share/rime-data";  // fallback
        }
    }
    traits.shared_data_dir = shared_dir.c_str();

    // Use default user data dir — term-ime's OWN XDG data dir (never ~/.rime or
    // the system rime user dir). rime stores its compiled prism/table + user db
    // here; user schema customizations also live here.
    std::string user_dir = user_data_dir_;
    if (user_dir.empty()) {
        const char* xdg_data = getenv("XDG_DATA_HOME");
        if (xdg_data && *xdg_data) {
            user_dir = std::filesystem::path(xdg_data) / "term-ime";
        } else {
            const char* home = getenv("HOME");
            if (home) {
                user_dir = std::filesystem::path(home) / ".local" / "share" / "term-ime";
            } else {
                user_dir = "/tmp/term-ime";
            }
        }
    }
    traits.user_data_dir = user_dir.c_str();

    traits.distribution_name = "term-ime";
    traits.distribution_code_name = "term-ime";
    traits.distribution_version = "1.0.0";
    traits.app_name = "rime.term-ime";

    // staging_dir (where compiled prism.bin/table.bin are written) and
    // prebuilt_data_dir (where prebuilt .bin are read from) MUST be absolute —
    // rime defaults them to the relative path "build", which resolves against
    // the process CWD and scatters deploy output to an unexpected location.
    std::string staging_dir = std::filesystem::path(user_dir) / "build";
    std::string prebuilt_dir = std::filesystem::path(shared_dir) / "build";
    traits.staging_dir = staging_dir.c_str();
    traits.prebuilt_data_dir = prebuilt_dir.c_str();

    rime_->setup(&traits);
    rime_->initialize(nullptr);

    // Force a full maintenance check: this compiles (deploys) the dictionary into
    // prism.bin/table.bin when missing or stale. Without it, on a fresh user data
    // dir rime has no compiled dictionary and input (e.g. pinyin) produces no
    // candidates. full_check=True ensures deployment runs even on first launch.
    if (rime_->start_maintenance(True)) {
        rime_->join_maintenance_thread();
    }

    // start_maintenance's workspace_update can fail to build schemas when the
    // shared data dir has no prebuilt build/ (our bundled rime-data ships source
    // .yaml, not compiled .bin). Explicitly deploy each schema file if its prism
    // is still missing in the user data dir's staging build/.
    if (!shared_dir.empty() && rime_->deploy_schema) {
        std::filesystem::path staging = std::filesystem::path(user_dir) / "build";
        for (auto& entry : std::filesystem::directory_iterator(shared_dir)) {
            auto p = entry.path();
            if (p.filename().string().find(".schema.yaml") == std::string::npos)
                continue;
            std::string stem = p.stem().string();
            std::string schema_id = stem.substr(0, stem.rfind(".schema"));
            std::string prism_name = schema_id + ".prism.bin";
            if (!std::filesystem::exists(staging / prism_name)) {
                spdlog::info("Deploying schema: {}", p.string());
                rime_->deploy_schema(p.string().c_str());
            }
        }
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
        // Clear composition after selection so state() returns Inactive
        rime_->clear_composition(session_);
        return result;
    }
    return U"";
}

void RimeIme::backspace() {
    if (!rime_ || !session_)
        return;
    // Send XK_BackSpace to rime — it deletes one syllable character from the
    // composition (rather than canceling the whole input like cancel() does).
    rime_->process_key(session_, 0xFF08, 0);  // XK_BackSpace
    update_state();
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