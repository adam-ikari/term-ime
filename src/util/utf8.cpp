#include "utf8.hpp"

char32_t utf8::decode(const uint8_t* data, size_t len, size_t& pos) {
    if (pos >= len) return U'\0';

    uint8_t first = data[pos++];
    int n = char_len(first);

    char32_t result = 0;

    if (n == 1) {
        return static_cast<char32_t>(first);
    }

    result = static_cast<char32_t>(first & (0xFF >> (n + 1)));

    for (int i = 1; i < n && pos < len; ++i) {
        result = (result << 6) | (data[pos++] & 0x3F);
    }

    return result;
}

std::string utf8::encode(char32_t ch) {
    std::string result;

    // Handle null or invalid characters
    if (ch == 0 || ch > 0x10FFFF) {
        return " ";  // Return space for invalid characters
    }

    if (ch < 0x80) {
        result += static_cast<char>(ch);
    } else if (ch < 0x800) {
        result += static_cast<char>(0xC0 | (ch >> 6));
        result += static_cast<char>(0x80 | (ch & 0x3F));
    } else if (ch < 0x10000) {
        result += static_cast<char>(0xE0 | (ch >> 12));
        result += static_cast<char>(0x80 | ((ch >> 6) & 0x3F));
        result += static_cast<char>(0x80 | (ch & 0x3F));
    } else {
        result += static_cast<char>(0xF0 | (ch >> 18));
        result += static_cast<char>(0x80 | ((ch >> 12) & 0x3F));
        result += static_cast<char>(0x80 | ((ch >> 6) & 0x3F));
        result += static_cast<char>(0x80 | (ch & 0x3F));
    }

    return result;
}

int utf8::width(char32_t ch) {
    // CJK Unified Ideographs
    if (ch >= 0x4E00 && ch <= 0x9FFF) return 2;
    // CJK Extensions
    if (ch >= 0x3400 && ch <= 0x4DBF) return 2;
    if (ch >= 0x20000 && ch <= 0x2A6DF) return 2;
    // Fullwidth forms
    if (ch >= 0xFF00 && ch <= 0xFFEF) return 2;
    // Hangul
    if (ch >= 0xAC00 && ch <= 0xD7AF) return 2;
    // Hiragana and Katakana
    if (ch >= 0x3040 && ch <= 0x30FF) return 2;

    return 1;
}
