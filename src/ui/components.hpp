#pragma once

#include "jsx.hpp"
#include "../ime/engine.hpp"
#include <string>
#include <vector>

// UI Components for term-ime
// Each component is a function that takes props and returns an Element

namespace ui {

// ============================================================================
// Candidate Bar Components
// ============================================================================

// Props for ModeIndicator
struct ModeIndicatorProps {
    std::string mode;
    bool is_chinese = false;
};

Element ModeIndicator(const ModeIndicatorProps& props);

// Props for CandidateItem
struct CandidateItemProps {
    int index;
    std::u32string text;
    bool selected = false;
    int scroll_offset = 0;  // Scroll offset in characters for overflow display
};

Element CandidateItem(const CandidateItemProps& props);

// Props for CandidateBar
struct CandidateBarProps {
    std::vector<Candidate> candidates;
    size_t selected = 0;
    std::string buffer;
    std::string mode;
};

Element CandidateBar(const CandidateBarProps& props);

// Props for EmptyBar (shown when no candidates)
struct EmptyBarProps {
    std::string mode;
};

Element EmptyBar(const EmptyBarProps& props);

// ============================================================================
// Status Bar Components
// ============================================================================

// Props for AIIndicator
struct AIIndicatorProps {
    bool enabled = false;
    bool loading = false;
    bool downloading = false;
    int download_progress = 0;
};

Element AIIndicator(const AIIndicatorProps& props);

// Props for StatusBar
struct StatusBarProps {
    std::string mode;
    std::string lang_name;
    bool ai_enabled = false;
    bool ai_loading = false;
    bool downloading = false;
    int download_progress = 0;
};

Element StatusBar(const StatusBarProps& props);

// ============================================================================
// Hints Components
// ============================================================================

struct HintItemProps {
    std::string key;
    std::string action;
};

Element HintItem(const HintItemProps& props);

Element HintsBar();

// ============================================================================
// Main UI Component
// ============================================================================

// Props for MainBar (combines status + candidates)
struct MainBarProps {
    std::string mode;
    std::string lang_name;
    std::vector<Candidate> candidates;
    size_t selected = 0;
    std::string buffer;
    bool ai_enabled = false;
    bool ai_loading = false;
    bool downloading = false;
    int download_progress = 0;
    int term_width = 80;    // Terminal width in columns
    int scroll_offset = 0;  // Scroll offset in characters for overflow
};

Element MainBar(const MainBarProps& props);

}  // namespace ui
