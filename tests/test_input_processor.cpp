#include "core/input_processor.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

// Helper: feed a sequence of bytes and collect the results.
static std::vector<input_sm::Result> feed(InputProcessor& p, const std::vector<uint8_t>& bytes) {
    std::vector<input_sm::Result> out;
    for (auto b : bytes)
        out.push_back(p.process(b));
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
    p.process(0x01);           // -> Prefix
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
    p.process(0x1b);          // -> Escape
    p.process('[');           // -> EscapeCSI
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
    p.process(0x1b);          // -> Escape
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
        EXPECT_FALSE(r.forward);             // still buffering, not forwarding
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

// ---- Parser boundary tests ----
TEST(InputProcessorTest, CtrlA_ForwardIsOnlyCtrlA_NotToggle) {
    InputProcessor p;
    p.process(0x01);
    // A digit after Ctrl+A should forward {0x01, digit}, not toggle
    auto r = p.process('1');
    EXPECT_TRUE(r.forward);
    ASSERT_EQ(r.data.size(), 2u);
    EXPECT_EQ(r.data[0], 0x01);
    EXPECT_EQ(r.data[1], '1');
    EXPECT_FALSE(r.toggle_mode);
}

TEST(InputProcessorTest, CtrlA_Digit_NotToggle) {
    InputProcessor p;
    p.process(0x01);
    auto r = p.process('2');
    EXPECT_TRUE(r.forward);
    EXPECT_FALSE(r.toggle_mode);
}

TEST(InputProcessorTest, CtrlA_UppercaseLetter_Forwards) {
    InputProcessor p;
    p.process(0x01);
    auto r = p.process('Z');
    EXPECT_TRUE(r.forward);
    ASSERT_EQ(r.data.size(), 2u);
    EXPECT_EQ(r.data[0], 0x01);
    EXPECT_EQ(r.data[1], 'Z');
}

TEST(InputProcessorTest, MultipleEscSequencesCorrectly) {
    InputProcessor p;
    // First ESC sequence
    p.process(0x1b);
    p.process('[');
    auto r1 = p.process('A');
    EXPECT_TRUE(r1.forward);
    EXPECT_FALSE(p.in_escape());

    // Second ESC sequence immediately after
    p.process(0x1b);
    p.process('[');
    auto r2 = p.process('B');
    EXPECT_TRUE(r2.forward);
    EXPECT_FALSE(p.in_escape());
}

TEST(InputProcessorTest, TwoConsecutiveEscSequences) {
    InputProcessor p;
    // Arrow up then arrow down
    auto rs1 = feed(p, {0x1b, '[', 'A'});
    EXPECT_TRUE(rs1[2].forward);
    EXPECT_FALSE(p.in_escape());

    auto rs2 = feed(p, {0x1b, '[', 'B'});
    EXPECT_TRUE(rs2[2].forward);
    EXPECT_FALSE(p.in_escape());
}

TEST(InputProcessorTest, NonAsciiBytesForwardNormally) {
    InputProcessor p;
    // High bytes (0x80+) should forward normally
    for (uint8_t b = 0x80; b < 0x90; ++b) {
        auto r = p.process(b);
        EXPECT_TRUE(r.forward);
        ASSERT_EQ(r.data.size(), 1u);
        EXPECT_EQ(r.data[0], b);
    }
}

TEST(InputProcessorTest, CtrlA_SpaceAfterOtherBytes_StillToggles) {
    InputProcessor p;
    // Send some normal bytes first
    p.process('h');
    p.process('e');
    // Then Ctrl+A + Space
    p.process(0x01);
    auto r = p.process(' ');
    EXPECT_TRUE(r.toggle_mode);
    EXPECT_FALSE(r.forward);
}

// ---- H4 regression: an escape final byte must not be mistaken for pinyin ----
// app.cpp decides "this byte just completed an ESC sequence" by checking
// forward && data[0] == 0x1b (the SM is back in Normal before the byte is
// examined, so in_escape() alone cannot tell). A DA reply ESC[?1;2c ends with
// lowercase 'c' and must arrive as one whole forwarded result so the IME never
// eats the final letter as a pinyin keystroke.
TEST(InputProcessorTest, DaReplyEscapeForwardsWhole) {
    InputProcessor p;
    auto rs = feed(p, {0x1b, '[', '?', '1', ';', '2', 'c'});
    // Only the final byte completes the sequence; the prefix bytes buffer.
    for (size_t i = 0; i + 1 < rs.size(); ++i) {
        EXPECT_FALSE(rs[i].forward);
    }
    const auto& r = rs.back();
    EXPECT_TRUE(r.forward);
    ASSERT_EQ(r.data.size(), 7u);
    EXPECT_EQ(r.data[0], 0x1b);  // the app-side "completed an escape" signal
    EXPECT_EQ(r.data[1], '[');
    EXPECT_EQ(r.data[6], 'c');
    EXPECT_FALSE(p.in_escape());
}

// ---- M1 regression: a control byte cannot be swallowed by a half-open CSI ----
// ESC[ + Ctrl+C used to park the byte in EscapeCSI (the unguarded buffer_byte
// transition matched first) until a later final flushed it out, swallowing the
// Ctrl+C. It must end the sequence and be forwarded on its own.
TEST(InputProcessorTest, ControlByteEndsHalfOpenCsi) {
    InputProcessor p;
    auto rs = feed(p, {0x1b, '[', 0x03, 'x'});
    EXPECT_FALSE(rs[0].forward);
    EXPECT_FALSE(rs[1].forward);
    // Ctrl+C is not swallowed: forwarded alone, sequence ended.
    EXPECT_TRUE(rs[2].forward);
    ASSERT_EQ(rs[2].data.size(), 1u);
    EXPECT_EQ(rs[2].data[0], 0x03);
    EXPECT_FALSE(p.in_escape());
    // The next key is an independent key, not a CSI tail.
    EXPECT_TRUE(rs[3].forward);
    ASSERT_EQ(rs[3].data.size(), 1u);
    EXPECT_EQ(rs[3].data[0], 'x');
}

// ---- M2 regression: ESC O + non-SS3 final forwards the byte on its own ----
// vim's application cursor mode sends ESC O A-D/F/H/P-S; a plain letter after
// ESC O (here 'x') used to hang in EscapeCSI and swallow the key.
TEST(InputProcessorTest, EscO_NonSsfinalForwardsKeyIndependently) {
    InputProcessor p;
    auto rs = feed(p, {0x1b, 'O', 'x'});
    EXPECT_FALSE(rs[0].forward);
    EXPECT_FALSE(rs[1].forward);
    EXPECT_TRUE(rs[2].forward);
    ASSERT_EQ(rs[2].data.size(), 1u);
    EXPECT_EQ(rs[2].data[0], 'x');  // forwarded alone, not glued to ESC O
    EXPECT_FALSE(p.in_escape());
}
