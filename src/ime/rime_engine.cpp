#include "rime_engine.hpp"
#include "../util/utf8.hpp"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>

// Bundled rime-data dir (set by CMake when USE_BUNDLED_DEPS sources are
// built). Empty string when building against system rime-data.
#ifndef RIME_BUNDLED_DATA_DIR
#define RIME_BUNDLED_DATA_DIR ""
#endif

// rime_life_ is built with an explicit deleter: GCC cannot default-construct a
// unique_ptr whose deleter is a nested class of the unique_ptr's owner.
RimeIme::RimeIme(const std::string& shared_data_dir, const std::string& user_data_dir)
    : rime_life_(nullptr, RimeShutdown{this}), shared_data_dir_(shared_data_dir), user_data_dir_(user_data_dir) {}

RimeIme::~RimeIme() = default;  // rime_life_ destroys the session and finalizes librime

void RimeIme::RimeShutdown::operator()(RimeApi* api) const {
    if (!api) {
        return;
    }
    RimeSessionId session = owner ? owner->session_ : 0;
    if (session) {
        api->destroy_session(session);
        owner->session_ = 0;
    }
    spdlog::debug("Rime: finalizing global state (session={})", session);
    api->finalize();
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
            user_local,             // user-local install
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
    resolved_shared_dir_ = shared_dir;
    resolved_user_dir_ = user_dir;

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
    // librime's global state exists from here on; own it so every exit path
    // (failures below, and ~RimeIme) releases it via finalize().
    rime_life_ = std::unique_ptr<RimeApi, RimeShutdown>(rime_, RimeShutdown{this});

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
        // Non-throwing filesystem calls: a missing/unreadable shared data dir must
        // not throw out of here with rime left initialized.
        std::filesystem::path staging = std::filesystem::path(user_dir) / "build";
        std::error_code ec;
        for (auto& entry : std::filesystem::directory_iterator(shared_dir, ec)) {
            auto p = entry.path();
            if (p.filename().string().find(".schema.yaml") == std::string::npos)
                continue;
            std::string stem = p.stem().string();
            std::string schema_id = stem.substr(0, stem.rfind(".schema"));
            std::string prism_name = schema_id + ".prism.bin";
            if (!std::filesystem::exists(staging / prism_name, ec)) {
                spdlog::info("Deploying schema: {}", p.string());
                rime_->deploy_schema(p.string().c_str());
            }
        }
        if (ec) {
            spdlog::warn("Rime: cannot scan shared data dir {}: {}", shared_dir, ec.message());
        }
    }

    // Create session
    session_ = rime_->create_session();
    if (!session_) {
        spdlog::error("Rime: failed to create session, releasing input method");
        rime_life_.reset();  // finalize() now; no session to destroy
        return false;
    }
    // Materialise any per-combination fuzzy schema (a strict subset of groups
    // enabled) now that rime is up, before the caller selects a schema.
    ensure_fuzzy_schema();
    return true;
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
    // Clear rime session composition when switching modes
    if (rime_ && session_) {
        rime_->clear_composition(session_);
    }
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

    // librime's digit keys select the candidates of the current page: '1'-'9'
    // for candidates 0-8 and '0' for candidate 9 (default select_keys
    // "1234567890"). Past index 9 there is no single-digit key; refuse rather
    // than synthesize an invalid one ('1'+9 is ':', which selects nothing).
    if (index < 0 || index > 9)
        return U"";
    char key = (index == 9) ? '0' : static_cast<char>('1' + index);
    rime_->process_key(session_, key, 0);

    // Get committed text
    std::u32string result;
    RIME_STRUCT(RimeCommit, commit);
    if (rime_->get_commit(session_, &commit)) {
        const char* text = commit.text;
        result = text ? utf8_to_utf32(text) : U"";
        rime_->free_commit(&commit);
    }

    // If state is still Composing/Selecting but buffer is empty, clear it.
    // Otherwise keep it so the user can continue typing the remaining input.
    if (state() != ImeState::Inactive && buffer().empty()) {
        rime_->clear_composition(session_);
    }
    return result;
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
    // Always clear composition to ensure clean state
    rime_->clear_composition(session_);
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

std::string RimeIme::fuzzy_signature() const {
    // All-on and all-off use the bundled schemas directly; only a strict subset
    // needs a generated per-combination schema, keyed by its enabled groups.
    static const std::vector<std::string> kAll = {"zh_z", "n_l", "r", "hu_f", "nose"};
    if (fuzzy_groups_.empty() || fuzzy_groups_ == kAll)
        return "";
    std::string sig;
    for (const auto& g : kAll) {
        if (std::find(fuzzy_groups_.begin(), fuzzy_groups_.end(), g) != fuzzy_groups_.end()) {
            if (!sig.empty())
                sig += "_";
            sig += g;
        }
    }
    return sig;
}

void RimeIme::ensure_fuzzy_schema() {
    // Called after rime is initialized: materialise the per-combination schema
    // (template pruned to the enabled groups) and deploy it when its prism is
    // missing. All-on/all-off use bundled schemas and need nothing here.
    const std::string sig = fuzzy_signature();
    if (sig.empty() || !rime_ || !rime_->deploy_schema)
        return;
    if (resolved_shared_dir_.empty() || resolved_user_dir_.empty())
        return;

    const std::string schema_id = "luna_pinyin_simp_fuzzy_" + sig;
    const std::filesystem::path out = std::filesystem::path(resolved_user_dir_) / (schema_id + ".schema.yaml");
    const std::filesystem::path staging =
        std::filesystem::path(resolved_user_dir_) / "build" / (schema_id + ".prism.bin");

    // Re-generate when the template changed (mtime newer than our copy).
    std::error_code ec;
    if (std::filesystem::exists(out, ec)) {
        const auto out_mtime = std::filesystem::last_write_time(out, ec);
        const auto tpl_mtime = std::filesystem::last_write_time(
            std::filesystem::path(resolved_shared_dir_) / "luna_pinyin_simp_fuzzy.schema.yaml", ec);
        if (!ec && out_mtime >= tpl_mtime && std::filesystem::exists(staging, ec)) {
            return;  // already generated and compiled
        }
    }

    // Prune the template: keep the enabled groups' rule blocks.
    std::ifstream in(std::filesystem::path(resolved_shared_dir_) / "luna_pinyin_simp_fuzzy.schema.yaml");
    if (!in)
        return;
    std::string line;
    std::string body;
    std::string block;     // current # BEGIN fuzzy:<block> .. # END fuzzy:<block> body
    std::string block_id;  // block id being collected (zh_z/n_l/r_l/r_y/hu_f/en_eng/an_ang)
    bool in_block = false;
    bool keep_block = false;
    auto flush_block = [&]() {
        if (!block.empty() && keep_block)
            body += block;
        block.clear();
        block_id.clear();
        keep_block = false;
    };
    // The settings toggles group several rule blocks: "r" covers r_l+r_y,
    // "nose" covers en_eng+an_ang. Map block id -> toggle group id.
    auto group_of = [](const std::string& bid) -> const char* {
        if (bid == "r_l" || bid == "r_y")
            return "r";
        if (bid == "en_eng" || bid == "an_ang")
            return "nose";
        return nullptr;  // zh_z/n_l/hu_f use their own id; unknown -> not a block
    };
    auto toggle_of = [&](const std::string& bid) -> std::string {
        if (const char* g = group_of(bid))
            return g;
        return bid;  // zh_z / n_l / hu_f
    };
    while (std::getline(in, line)) {
        if (!in_block) {
            const auto pos = line.find("# BEGIN fuzzy:");
            if (pos != std::string::npos) {
                const std::string id = line.substr(pos + 14, line.find_first_of(" \t", pos + 14) - (pos + 14));
                // Only recognised block ids open a block; a comment that merely
                // mentions the marker text must not (it would swallow the block).
                if (!id.empty() && (group_of(id) != nullptr || id == "zh_z" || id == "n_l" || id == "hu_f")) {
                    in_block = true;
                    block = line + "\n";
                    block_id = id;
                    const std::string toggle = toggle_of(id);
                    keep_block = std::find(fuzzy_groups_.begin(), fuzzy_groups_.end(), toggle) != fuzzy_groups_.end();
                    continue;
                }
            }
            body += line + "\n";
        } else if (line.find("# END fuzzy:") != std::string::npos) {
            block += line + "\n";
            flush_block();
            in_block = false;
        } else {
            block += line + "\n";
        }
    }
    flush_block();
    if (in_block)
        body += block;  // unterminated block: keep it verbatim

    // Point the generated schema at its own id/prism.
    const std::string old_id = "luna_pinyin_simp_fuzzy";
    size_t at = 0;
    while ((at = body.find(old_id, at)) != std::string::npos) {
        body.replace(at, old_id.size(), schema_id);
        at += schema_id.size();
    }

    std::error_code mkdir_ec;
    std::filesystem::create_directories(resolved_user_dir_, mkdir_ec);
    std::ofstream out_file(out);
    if (!out_file) {
        spdlog::warn("Rime: cannot write generated schema {}", out.string());
        return;
    }
    out_file << body;
    spdlog::info("Rime: wrote generated fuzzy schema {}", out.string());

    // Deploy when the prism is missing (or the schema changed).
    if (!std::filesystem::exists(staging, ec)) {
        spdlog::info("Rime: deploying generated schema {}", out.string());
        rime_->deploy_schema(out.string().c_str());
    }
}

std::string RimeIme::fuzzy_variant(const std::string& schema_id) const {
    // Only the bundled simplified schema has a fuzzy twin; other schemas keep
    // their id, so the toggles simply have no effect on them.
    if (schema_id != "luna_pinyin_simp")
        return schema_id;
    const std::string sig = fuzzy_signature();
    if (sig.empty())
        return fuzzy_groups_.empty() ? schema_id : "luna_pinyin_simp_fuzzy";
    return "luna_pinyin_simp_fuzzy_" + sig;
}

void RimeIme::set_fuzzy_groups(const std::vector<std::string>& groups) {
    fuzzy_groups_ = groups;
    if (rime_ && rime_life_ && session_) {
        // Live engine: materialise any generated schema now; the caller
        // re-selects through fuzzy_variant() afterwards.
        ensure_fuzzy_schema();
    }
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