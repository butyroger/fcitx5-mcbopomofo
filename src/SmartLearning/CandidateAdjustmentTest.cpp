#include "CandidateAdjustment.h"

#include "gtest/gtest.h"

namespace McBopomofo::SmartLearning {
namespace {

TEST(CandidateAdjustmentTest, SingleCharacterIsAdjustable) {
  EXPECT_EQ(CheckCandidateAdjustable("ㄋㄧˇ", "你", "你"),
            AdjustCheck::kAllowed);
}

TEST(CandidateAdjustmentTest, MultiCharacterPhraseIsAdjustable) {
  EXPECT_EQ(CheckCandidateAdjustable("ㄋㄧˇ-ㄏㄠˇ", "你好", "你好"),
            AdjustCheck::kAllowed);
}

TEST(CandidateAdjustmentTest, PunctuationIsRejectedWithError) {
  EXPECT_EQ(CheckCandidateAdjustable("_punctuation_<", "，", "，"),
            AdjustCheck::kRejectWithError);
  EXPECT_EQ(CheckCandidateAdjustable("_half_punctuation_,", ",", ","),
            AdjustCheck::kRejectWithError);
  EXPECT_EQ(CheckCandidateAdjustable("_ctrl_punctuation_1", "x", "x"),
            AdjustCheck::kRejectWithError);
}

TEST(CandidateAdjustmentTest, LiteralAndEnglishAreRejectedWithError) {
  EXPECT_EQ(CheckCandidateAdjustable("_literal_a", "a", "a"),
            AdjustCheck::kRejectWithError);
  EXPECT_EQ(CheckCandidateAdjustable("_english_hello", "hello", "hello"),
            AdjustCheck::kRejectWithError);
  EXPECT_EQ(CheckCandidateAdjustable("_letter_A", "A", "A"),
            AdjustCheck::kRejectWithError);
  EXPECT_EQ(CheckCandidateAdjustable("_number_1", "1", "1"),
            AdjustCheck::kRejectWithError);
}

TEST(CandidateAdjustmentTest, SymbolTableReadingIsRejectedWithError) {
  EXPECT_EQ(CheckCandidateAdjustable("_punctuation_list", "※", "※"),
            AdjustCheck::kRejectWithError);
  EXPECT_EQ(CheckCandidateAdjustable("_anything_else", "x", "x"),
            AdjustCheck::kRejectWithError);
}

TEST(CandidateAdjustmentTest, ExpandedMacroIsIgnoredSilently) {
  EXPECT_EQ(CheckCandidateAdjustable("ㄐㄧㄣ-ㄊㄧㄢ", "2026-01-01",
                                     "MACRO@DATE_TODAY_SHORT"),
            AdjustCheck::kIgnoreSilently);
}

TEST(CandidateAdjustmentTest, MacroCheckComesBeforeReadingCheck) {
  EXPECT_EQ(CheckCandidateAdjustable("_punctuation_x", "a", "MACRO@X"),
            AdjustCheck::kIgnoreSilently);
}

TEST(CandidateAdjustmentTest, EmptyOrSpacedValueIsRejectedWithError) {
  EXPECT_EQ(CheckCandidateAdjustable("ㄋㄧˇ", "", ""),
            AdjustCheck::kRejectWithError);
  EXPECT_EQ(CheckCandidateAdjustable("ㄋㄧˇ", "a b", "a b"),
            AdjustCheck::kRejectWithError);
  EXPECT_EQ(CheckCandidateAdjustable("ㄋㄧˇ", "a\tb", "a\tb"),
            AdjustCheck::kRejectWithError);
  EXPECT_EQ(CheckCandidateAdjustable("ㄋㄧˇ", "a\nb", "a\nb"),
            AdjustCheck::kRejectWithError);
  EXPECT_EQ(CheckCandidateAdjustable("ㄋㄧˇ", "a　b", "a　b"),
            AdjustCheck::kRejectWithError);
  EXPECT_EQ(CheckCandidateAdjustable("ㄋㄧˇ", "a b", "a b"),
            AdjustCheck::kRejectWithError);
}

TEST(CandidateAdjustmentTest, PageOfCandidateIndex) {
  // 11 candidates, 9 a page: the last one (index 10) is on page 1.
  EXPECT_EQ(PageOfCandidateIndex(0, 9), 0u);
  EXPECT_EQ(PageOfCandidateIndex(8, 9), 0u);
  EXPECT_EQ(PageOfCandidateIndex(9, 9), 1u);
  EXPECT_EQ(PageOfCandidateIndex(10, 9), 1u);
  EXPECT_EQ(PageOfCandidateIndex(5, 0), 0u);
}

}  // namespace
}  // namespace McBopomofo::SmartLearning
