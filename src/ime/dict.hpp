#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>

// Word entry with frequency for ranking
struct DictEntry {
    std::u32string word;
    int frequency = 0;
};

// Dictionary format type
enum class DictFormat {
    Auto,       // Auto-detect from file extension
    Plain,      // Simple format: pinyin<TAB>word
    Rime,       // Rime format: YAML-based
    Sougou,     // Sougou cell dict (.scel)
    Custom      // Custom format with callback
};

class Dict {
public:
    Dict() = default;

    // Load dictionary from file
    bool load(const std::string& path, DictFormat format = DictFormat::Auto);

    // Load from multiple files
    bool load_multiple(const std::vector<std::string>& paths);

    // Import words from string (for runtime import)
    void import(const std::string& pinyin, const std::u32string& word, int frequency = 0);

    // Import from plain text string
    bool import_plain(const std::string& content);

    // Query candidates by pinyin
    std::vector<std::u32string> query(const std::string& pinyin) const;

    // Query with frequency info
    std::vector<DictEntry> query_with_freq(const std::string& pinyin) const;

    // Get statistics
    size_t entry_count() const;
    size_t pinyin_count() const;

    // Clear all entries
    void clear();

private:
    bool load_plain(const std::string& path);
    bool load_rime(const std::string& path);
    bool detect_format(const std::string& path, DictFormat& format);

    // pinyin -> [entries...]
    std::unordered_map<std::string, std::vector<DictEntry>> entries_;
};
