#include <gtest/gtest.h>
#include "text_navigation.h"

class TextNavigationTest : public ::testing::Test {
protected:
    const char* simple_text = "Hello World";
    const char* multiline_text = "Line one\nLine two\nLine three";
    const char* paragraph_text = "Para one\n\nPara two\n\nPara three";
};

// find_line_start tests
TEST_F(TextNavigationTest, FindLineStart_AtBeginning) {
    EXPECT_EQ(find_line_start(simple_text, 0), 0);
}

TEST_F(TextNavigationTest, FindLineStart_InMiddleOfLine) {
    EXPECT_EQ(find_line_start(simple_text, 5), 0);
}

TEST_F(TextNavigationTest, FindLineStart_SecondLine) {
    EXPECT_EQ(find_line_start(multiline_text, 12), 9);
}

TEST_F(TextNavigationTest, FindLineStart_AtNewline) {
    EXPECT_EQ(find_line_start(multiline_text, 8), 0);
}

// find_line_end tests
TEST_F(TextNavigationTest, FindLineEnd_FirstLine) {
    int len = strlen(multiline_text);
    EXPECT_EQ(find_line_end(multiline_text, len, 0), 8);
}

TEST_F(TextNavigationTest, FindLineEnd_SecondLine) {
    int len = strlen(multiline_text);
    EXPECT_EQ(find_line_end(multiline_text, len, 9), 17);
}

TEST_F(TextNavigationTest, FindLineEnd_LastLine) {
    int len = strlen(multiline_text);
    EXPECT_EQ(find_line_end(multiline_text, len, 18), len);
}

// find_prev_word_start tests
TEST_F(TextNavigationTest, FindPrevWordStart_AtBeginning) {
    EXPECT_EQ(find_prev_word_start(simple_text, 0), 0);
}

TEST_F(TextNavigationTest, FindPrevWordStart_InSecondWord) {
    EXPECT_EQ(find_prev_word_start(simple_text, 8), 6);
}

TEST_F(TextNavigationTest, FindPrevWordStart_AtSpaceBetweenWords) {
    EXPECT_EQ(find_prev_word_start(simple_text, 6), 0);
}

// find_next_word_end tests
TEST_F(TextNavigationTest, FindNextWordEnd_AtBeginning) {
    int len = strlen(simple_text);
    EXPECT_EQ(find_next_word_end(simple_text, len, 0), 5);
}

TEST_F(TextNavigationTest, FindNextWordEnd_InMiddleOfWord) {
    int len = strlen(simple_text);
    EXPECT_EQ(find_next_word_end(simple_text, len, 2), 5);
}

TEST_F(TextNavigationTest, FindNextWordEnd_AtSpace) {
    int len = strlen(simple_text);
    EXPECT_EQ(find_next_word_end(simple_text, len, 5), 11);
}

TEST_F(TextNavigationTest, FindNextWordEnd_AtEnd) {
    int len = strlen(simple_text);
    EXPECT_EQ(find_next_word_end(simple_text, len, len), len);
}

// find_paragraph_start tests
TEST_F(TextNavigationTest, FindParagraphStart_AtBeginning) {
    EXPECT_EQ(find_paragraph_start(paragraph_text, 0), 0);
}

TEST_F(TextNavigationTest, FindParagraphStart_InFirstPara) {
    EXPECT_EQ(find_paragraph_start(paragraph_text, 5), 0);
}

// find_paragraph_end tests
TEST_F(TextNavigationTest, FindParagraphEnd_FromStart) {
    int len = strlen(paragraph_text);
    int result = find_paragraph_end(paragraph_text, len, 0);
    EXPECT_GT(result, 8);
}

TEST_F(TextNavigationTest, FindParagraphEnd_AtEnd) {
    int len = strlen(paragraph_text);
    EXPECT_EQ(find_paragraph_end(paragraph_text, len, len), len);
}

// KeyRepeatState tests
TEST(KeyRepeatStateTest, NewPress_ShouldAct) {
    KeyRepeatState state;
    EXPECT_TRUE(state.should_act(true, 0));
}

TEST(KeyRepeatStateTest, KeyReleased_ShouldNotAct) {
    KeyRepeatState state;
    state.should_act(true, 0);
    EXPECT_FALSE(state.should_act(false, 100));
}

TEST(KeyRepeatStateTest, HeldKey_BeforeInitialDelay_ShouldNotAct) {
    KeyRepeatState state;
    state.should_act(true, 0);
    EXPECT_FALSE(state.should_act(true, 500));
}

TEST(KeyRepeatStateTest, HeldKey_AfterInitialDelay_ShouldAct) {
    KeyRepeatState state;
    state.should_act(true, 0);
    EXPECT_FALSE(state.should_act(true, 500));
    EXPECT_TRUE(state.should_act(true, 900));
}

TEST(KeyRepeatStateTest, HeldKey_RepeatInterval) {
    KeyRepeatState state;
    state.should_act(true, 0);
    state.should_act(true, 800);
    EXPECT_TRUE(state.should_act(true, 900));
    EXPECT_FALSE(state.should_act(true, 950));
    EXPECT_TRUE(state.should_act(true, 1000));
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
