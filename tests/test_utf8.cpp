#include <gtest/gtest.h>
#include "util/utf8.hpp"

class Utf8Test : public ::testing::Test {
protected:
    void SetUp() override {}
};

TEST_F(Utf8Test, EncodeAscii) {
    EXPECT_EQ(utf8::encode(U'A'), "A");
    EXPECT_EQ(utf8::encode(U'Z'), "Z");
    EXPECT_EQ(utf8::encode(U'0'), "0");
}

TEST_F(Utf8Test, EncodeChinese) {
    // "中" in UTF-8 is E4 B8 AD
    std::string result = utf8::encode(U'中');
    EXPECT_EQ(result.size(), 3);
    EXPECT_EQ((unsigned char)result[0], 0xE4);
    EXPECT_EQ((unsigned char)result[1], 0xB8);
    EXPECT_EQ((unsigned char)result[2], 0xAD);
}

TEST_F(Utf8Test, EncodeEmoji) {
    // 😀 U+1F600 in UTF-8 is F0 9F 98 80
    std::string result = utf8::encode(U'😀');
    EXPECT_EQ(result.size(), 4);
}

TEST_F(Utf8Test, DecodeAscii) {
    const uint8_t data[] = {'A', 'B', 'C'};
    size_t pos = 0;
    EXPECT_EQ(utf8::decode(data, 3, pos), U'A');
    EXPECT_EQ(pos, 1);
    EXPECT_EQ(utf8::decode(data, 3, pos), U'B');
    EXPECT_EQ(pos, 2);
}

TEST_F(Utf8Test, DecodeChinese) {
    // "中" in UTF-8
    const uint8_t data[] = {0xE4, 0xB8, 0xAD};
    size_t pos = 0;
    EXPECT_EQ(utf8::decode(data, 3, pos), U'中');
    EXPECT_EQ(pos, 3);
}

TEST_F(Utf8Test, RoundTrip) {
    char32_t chars[] = {U'A', U'中', U'日', U'😀', U'€'};
    for (char32_t ch : chars) {
        std::string encoded = utf8::encode(ch);
        const uint8_t* data = reinterpret_cast<const uint8_t*>(encoded.data());
        size_t pos = 0;
        char32_t decoded = utf8::decode(data, encoded.size(), pos);
        EXPECT_EQ(decoded, ch) << "Round trip failed for character";
    }
}
