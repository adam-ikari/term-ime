#include "core/input_processor.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

// Helper: feed a sequence of bytes and collect the results.
static std::vector<input_sm::Result> feed(InputProcessor& p, const std::vector<uint8_t>& bytes) {
    std::vector<input_sm::Result> out;
    for (auto b : bytes) out.push_back(p.process(b));
    return out;
}

// ---- Normal state: plain bytes forward one-to-one ----
TEST(InputProcessorTest, PlainByteForwards) {
    InputProcessor p;
    auto r = p.process('a');
    EXPECT_TRUE(r.forward);
    ASSERT_EQ(r.data.size(), 1u);
    EXPECT_EQ(r.data[0], 'a');
    EXPECT_FALSE(r.toggle_mode);
}

TEST(InputProcessorTest, MultiplePlainBytesEachForward) {
    InputProcessor p;
    auto rs = feed(p, {'a', 'b', 'c'});
    for (const auto& r : rs) {
        EXPECT_TRUE(r.forward);
        ASSERT_EQ(r.data.size(), 1u);
    }
    EXPECT_EQ(rs[0].data[0], 'a');
    EXPECT_EQ(rs[1].data[0], 'b');
    EXPECT_EQ(rs[2].data[0], 'c');
}

// ---- Ctrl+A prefix: the next byte decides ----
TEST(InputProcessorTest, CtrlA_Space_TogglesMode) {
    InputProcessor p;
    auto r1 = p.process(0x01);  // Ctrl+A -> Prefix
    EXPECT_FALSE(r1.forward);
    EXPECT_FALSE(r1.toggle_mode);

    auto r2 = p.process(' ');  // Space -> toggle
    EXPECT_TRUE(r2.toggle_mode);
    EXPECT_FALSE(r2.forward);
}

TEST(InputProcessorTest, CtrlA_A_ForwardsPrefix) {
    // Ctrl+A followed by 'A' (lowercase 'a' is the AI toggle in App, but the SM
    // itself just forwards {0x01, 'a'}; uppercase 'A' is a generic forward).
    InputProcessor p;
    p.process(0x01);
    auto r = p.process('A');
    EXPECT_TRUE(r.forward);
    ASSERT_EQ(r.data.size(), 2u);
    EXPECT_EQ(r.data[0], 0x01);
    EXPECT_EQ(r.data[1], 'A');
}

TEST(InputProcessorTest, CtrlA_CtrlA_ForwardsLiteralCtrlA) {
    InputProcessor p;
    p.process(0x01);  // -> Prefix
    auto r = p.process(0x01);  // second Ctrl+A -> literal 0x01
    EXPECT_TRUE(r.forward);
    ASSERT_EQ(r.data.size(), 1u);
    EXPECT_EQ(r.data[0], 0x01);
}

TEST(InputProcessorTest, CtrlA_Other_ForwardsTwoBytes) {
    InputProcessor p;
    p.process(0x01);
    auto r = p.process('x');
    EXPECT_TRUE(r.forward);
    ASSERT_EQ(r.data.size(), 2u);
    EXPECT_EQ(r.data[0], 0x01);
    EXPECT_EQ(r.data[1], 'x');
}

// ---- Escape sequences: reassembly ----
TEST(InputProcessorTest, LoneEscapeDoesNotForward) {
    InputProcessor p;
    auto r = p.process(0x1b);  // ESC -> Escape state
    EXPECT_FALSE(r.forward);
    EXPECT_TRUE(p.in_escape());
}

TEST(InputProcessorTest, EscapeThenCsiTerminatorForwardsWhole) {
    InputProcessor p;
    p.process(0x1b);  // -> Escape
    p.process('[');   // -> EscapeCSI
    auto r = p.process('A');  // terminator -> forward ESC[A
    EXPECT_TRUE(r.forward);
    ASSERT_EQ(r.data.size(), 3u);
    EXPECT_EQ(r.data[0], 0x1b);
    EXPECT_EQ(r.data[1], '[');
    EXPECT_EQ(r.data[2], 'A');
    EXPECT_FALSE(p.in_escape());  // back to Normal
}

TEST(InputProcessorTest, ApplicationModeArrowEscO_Forwards) {
    InputProcessor p;
    p.process(0x1b);
    p.process('O');  // -> EscapeCSI (application mode)
    auto r = p.process('B');
    EXPECT_TRUE(r.forward);
    ASSERT_EQ(r.data.size(), 3u);
    EXPECT_EQ(r.data[1], 'O');
    EXPECT_EQ(r.data[2], 'B');
}

TEST(InputProcessorTest, EscapeThenNonCsiForwardsTwoBytes) {
    InputProcessor p;
    p.process(0x1b);  // -> Escape
    auto r = p.process('x');  // not '['/'O' -> forward ESC x
    EXPECT_TRUE(r.forward);
    ASSERT_EQ(r.data.size(), 2u);
    EXPECT_EQ(r.data[0], 0x1b);
    EXPECT_EQ(r.data[1], 'x');
    EXPECT_FALSE(p.in_escape());
}

TEST(InputProcessorTest, CsiWithParamsTerminatesCorrectly) {
    InputProcessor p;
    p.process(0x1b);
    p.process('[');
    p.process('1');
    p.process(';');
    p.process('2');
    auto r = p.process('A');  // ESC[1;2A
    EXPECT_TRUE(r.forward);
    ASSERT_EQ(r.data.size(), 6u);
    EXPECT_EQ(r.data[5], 'A');
}

// ---- F4 regression: EscapeCSI buffer is capped ----
// Feeding a flood of non-terminator bytes must not crash or grow unbounded.
TEST(InputProcessorTest, EscapeCsiBufferIsCapped) {
    InputProcessor p;
    p.process(0x1b);
    p.process('[');
    // Feed far more non-terminator bytes than the cap; should stay in EscapeCSI
    // without exploding.
    for (int i = 0; i < 5000; ++i) {
        auto r = p.process('0' + (i % 10));  // digits are non-terminators
        EXPECT_FALSE(r.forward);  // still buffering, not forwarding
    }
    EXPECT_TRUE(p.in_escape());
    // A terminator still resolves the sequence (cap reset it but we continue).
    auto r = p.process('A');
    EXPECT_TRUE(r.forward);
    EXPECT_FALSE(p.in_escape());
}

// ---- reset() returns to Normal ----
TEST(InputProcessorTest, ResetClearsEscapeState) {
    InputProcessor p;
    p.process(0x1b);
    EXPECT_TRUE(p.in_escape());
    p.reset();
    EXPECT_FALSE(p.in_escape());
    // After reset, a plain byte forwards normally (not treated as ESC tail).
    auto r = p.process('a');
    EXPECT_TRUE(r.forward);
    ASSERT_EQ(r.data.size(), 1u);
}
