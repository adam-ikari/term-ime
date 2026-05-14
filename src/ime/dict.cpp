#include "dict.hpp"
#include "../util/utf8.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>

bool Dict::load(const std::string& path, DictFormat format) {
    if (format == DictFormat::Auto) {
        if (!detect_format(path, format)) {
            return false;
        }
    }

    switch (format) {
        case DictFormat::Plain:
        case DictFormat::Auto:
            return load_plain(path);
        case DictFormat::Rime:
            return load_rime(path);
        default:
            return load_plain(path);
    }
}

bool Dict::load_multiple(const std::vector<std::string>& paths) {
    bool success = true;
    for (const auto& path : paths) {
        if (!load(path)) {
            success = false;
        }
    }
    return success;
}

void Dict::import(const std::string& pinyin, const std::u32string& word, int frequency) {
    DictEntry entry{word, frequency};

    auto& list = entries_[pinyin];

    // Check if word already exists
    for (auto& e : list) {
        if (e.word == word) {
            e.frequency = std::max(e.frequency, frequency);
            return;
        }
    }

    list.push_back(entry);
}

bool Dict::import_plain(const std::string& content) {
    std::istringstream stream(content);
    std::string line;

    while (std::getline(stream, line)) {
        if (line.empty() || line[0] == '#') continue;

        size_t tab = line.find('\t');
        if (tab == std::string::npos) continue;

        std::string pinyin = line.substr(0, tab);
        std::string word = line.substr(tab + 1);

        // Parse optional frequency
        int freq = 0;
        size_t tab2 = word.find('\t');
        if (tab2 != std::string::npos) {
            freq = std::stoi(word.substr(tab2 + 1));
            word = word.substr(0, tab2);
        }

        // Convert UTF-8 to UTF-32
        std::u32string word32;
        const uint8_t* data = reinterpret_cast<const uint8_t*>(word.data());
        size_t pos = 0;
        while (pos < word.size()) {
            word32 += utf8::decode(data, word.size(), pos);
        }

        import(pinyin, word32, freq);
    }

    return true;
}

std::vector<std::u32string> Dict::query(const std::string& pinyin) const {
    auto it = entries_.find(pinyin);
    if (it == entries_.end()) {
        return {};
    }

    // Sort by frequency (descending)
    std::vector<DictEntry> sorted = it->second;
    std::sort(sorted.begin(), sorted.end(),
        [](const DictEntry& a, const DictEntry& b) {
            return a.frequency > b.frequency;
        });

    std::vector<std::u32string> result;
    for (const auto& e : sorted) {
        result.push_back(e.word);
    }
    return result;
}

std::vector<DictEntry> Dict::query_with_freq(const std::string& pinyin) const {
    auto it = entries_.find(pinyin);
    if (it == entries_.end()) {
        return {};
    }

    std::vector<DictEntry> sorted = it->second;
    std::sort(sorted.begin(), sorted.end(),
        [](const DictEntry& a, const DictEntry& b) {
            return a.frequency > b.frequency;
        });

    return sorted;
}

size_t Dict::entry_count() const {
    size_t count = 0;
    for (const auto& [pinyin, list] : entries_) {
        count += list.size();
    }
    return count;
}

size_t Dict::pinyin_count() const {
    return entries_.size();
}

void Dict::clear() {
    entries_.clear();
}

bool Dict::load_plain(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return false;
    }

    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());

    return import_plain(content);
}

bool Dict::load_rime(const std::string& path) {
    // TODO: Implement Rime YAML format parsing
    // For now, fall back to plain format
    return load_plain(path);
}

bool Dict::detect_format(const std::string& path, DictFormat& format) {
    // Simple detection by extension
    if (path.size() > 5 && path.substr(path.size() - 5) == ".scel") {
        format = DictFormat::Sougou;
        return true;
    }
    if (path.size() > 5 && path.substr(path.size() - 5) == ".yaml") {
        format = DictFormat::Rime;
        return true;
    }

    // Default to plain format
    format = DictFormat::Plain;
    return true;
}