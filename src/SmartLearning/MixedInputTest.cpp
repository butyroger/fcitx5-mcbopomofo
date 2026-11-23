#include "MixedInput.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace McBopomofo::SmartLearning {

namespace {
std::vector<std::string> Letters(const std::string& s) {
  std::vector<std::string> v;
  for (char c : s) v.push_back(std::string("_literal_") + c);
  return v;
}
}  // namespace

TEST(MixedInputTest, Classifiers) {
  EXPECT_TRUE(IsEnglishReading("_literal_a"));
  EXPECT_TRUE(IsEnglishReading("_english_hello"));
  EXPECT_FALSE(IsEnglishReading("_literal_3"));
  EXPECT_FALSE(IsEnglishReading("_letter_A"));
  EXPECT_TRUE(IsHalfWidthAlphanumericReading("_literal_3"));
  EXPECT_TRUE(IsHalfWidthAlphanumericReading("_numpad_7"));
  EXPECT_TRUE(IsChineseReading("ㄏㄠˇ"));
  EXPECT_TRUE(IsChineseReading("_literal_好"));
  EXPECT_FALSE(IsChineseReading("_literal_３"));
  EXPECT_FALSE(IsChineseReading(" "));
  EXPECT_TRUE(IsLetterReading("_literal_h"));
  EXPECT_FALSE(IsLetterReading("_english_hello"));
  EXPECT_FALSE(IsLetterReading("_literal_ "));
}

TEST(MixedInputTest, TrailingLetters) {
  std::vector<std::string> r{"ㄏㄠˇ", "_literal_ ", "_literal_h", "_literal_i"};
  EXPECT_EQ(TrailingLetters(r, 4), "hi");
  EXPECT_EQ(TrailingLetters(r, 3), "h");
  EXPECT_EQ(TrailingLetters(r, 2), "");
  r.push_back("_english_hello");
  EXPECT_EQ(TrailingLetters(r, 5), "");
}

TEST(MixedInputTest, AutoSpace) {
  std::vector<std::string> zh{"ㄋㄧˇ", "ㄏㄠˇ"};
  EXPECT_TRUE(AutoSpaceNeeded(zh, 2, /*beforeEnglish=*/true));
  EXPECT_FALSE(AutoSpaceNeeded(zh, 0, true));
  std::vector<std::string> en{"_literal_o", "_literal_k"};
  EXPECT_TRUE(AutoSpaceNeeded(en, 2, /*beforeEnglish=*/false));
  std::vector<std::string> digits{"_literal_2", "_literal_0"};
  EXPECT_FALSE(AutoSpaceNeeded(digits, 2, false));
  EXPECT_FALSE(AutoSpaceNeeded({"ㄏㄠˇ", "_literal_a"}, 1, false));
}

TEST(MixedInputTest, RemoveAutoSpace) {
  EXPECT_TRUE(
      ShouldRemoveAutoSpaceAfterDeletingEnglish({"ㄏㄠˇ", "_literal_ "}, 2));
  EXPECT_FALSE(ShouldRemoveAutoSpaceAfterDeletingEnglish(
      {"ㄏㄠˇ", "_literal_ ", "_literal_x"}, 2));
  EXPECT_FALSE(
      ShouldRemoveAutoSpaceAfterDeletingEnglish({"_literal_3", "_literal_ "}, 2));
  EXPECT_FALSE(ShouldRemoveAutoSpaceAfterDeletingEnglish({"_literal_ "}, 1));
}

TEST(MixedInputTest, TrimPlan) {
  auto isWord = [](const std::string& w) { return w == "hello" || w == "ok"; };
  EXPECT_EQ(PlanTrimBeforeCursor(Letters("hellojkq"), 8, isWord), 3u);
  EXPECT_EQ(PlanTrimBeforeCursor(Letters("ok"), 2, isWord), 0u);
  std::vector<std::string> mixed{"ㄐㄧㄣ",      "ㄊㄧㄢ",      "_literal_ ",
                                 "_literal_j", "_literal_q", "_literal_v",
                                 "_literal_x"};
  EXPECT_EQ(PlanTrimBeforeCursor(mixed, 7, isWord), 5u);
  std::vector<std::string> withDigit{"_literal_3", "_punctuation_,"};
  EXPECT_EQ(PlanTrimBeforeCursor(withDigit, 2, isWord), 1u);
  EXPECT_EQ(PlanTrimBeforeCursor(
                Letters("lsqq"), 4,
                [](const std::string& w) { return w == "ls"; }),
            4u);
}

TEST(MixedInputTest, ModeLabels) {
  EXPECT_EQ(LanguageModeLabel(false, false), "中");
  EXPECT_EQ(LanguageModeLabel(true, false), "英");
  EXPECT_EQ(LanguageModeLabel(false, true), "英");
  EXPECT_EQ(WidthModeLabel(true), "半");
  EXPECT_EQ(WidthModeLabel(false), "全");
}

TEST(MixedInputTest, SwitchReminderOnlyWhenCapsLockChanges) {
  EXPECT_FALSE(SwitchReminderText(true, false, false).has_value());
  EXPECT_FALSE(SwitchReminderText(true, true, true).has_value());
  EXPECT_EQ(SwitchReminderText(true, false, true), "ABC");
  EXPECT_EQ(SwitchReminderText(true, true, false), "中文");
}

TEST(MixedInputTest, SwitchReminderSkipsTheFirstStateAfterActivation) {
  // Caps Lock is already on at the first event: adopt it, no tip.
  bool known = false;
  bool caps = false;
  auto step = [&](bool now) {
    auto r = SwitchReminderText(known, caps, now);
    caps = now;
    known = true;
    return r;
  };
  EXPECT_FALSE(step(true).has_value());
  EXPECT_FALSE(step(true).has_value());
  EXPECT_EQ(step(false), "中文");
  EXPECT_FALSE(step(false).has_value());
  EXPECT_EQ(step(true), "ABC");
}

TEST(MixedInputTest, EnglishCompletionPlan) {
  std::vector<std::string> r{"ㄏㄠˇ", "_literal_ ", "_literal_h", "_literal_e",
                             "_literal_l"};
  auto plan = PlanEnglishCompletion(r, 5);
  ASSERT_TRUE(plan.has_value());
  EXPECT_EQ(plan->lettersToDelete, 3u);
  EXPECT_EQ(plan->englishReading, "_english_hel");
  plan = PlanEnglishCompletion(r, 4);
  ASSERT_TRUE(plan.has_value());
  EXPECT_EQ(plan->englishReading, "_english_he");
  // One letter, or none, is not enough.
  EXPECT_FALSE(PlanEnglishCompletion(r, 3).has_value());
  EXPECT_FALSE(PlanEnglishCompletion(r, 2).has_value());
  EXPECT_FALSE(PlanEnglishCompletion({}, 0).has_value());
  // A word picked from the completions is not letters.
  r.push_back("_english_hello");
  EXPECT_FALSE(PlanEnglishCompletion(r, 6).has_value());
  // Digits stop the run.
  EXPECT_FALSE(PlanEnglishCompletion(Letters("a1"), 2).has_value());
  auto mixed = Letters("1ab");
  plan = PlanEnglishCompletion(mixed, 3);
  ASSERT_TRUE(plan.has_value());
  EXPECT_EQ(plan->lettersToDelete, 2u);
  EXPECT_EQ(plan->englishReading, "_english_ab");
}

TEST(MixedInputTest, FullWidthSymbol) {
  EXPECT_EQ(FullWidthSymbol('/'), "／");
  EXPECT_EQ(FullWidthSymbol('!'), "！");
  EXPECT_EQ(FullWidthSymbol('~'), "～");
  EXPECT_EQ(FullWidthSymbol('='), "＝");
  EXPECT_EQ(FullWidthSymbol('a'), "ａ");
  // Outside the printable range, the character is kept.
  EXPECT_EQ(FullWidthSymbol(' '), " ");
  EXPECT_EQ(FullWidthSymbol('\t'), "\t");
}

}  // namespace McBopomofo::SmartLearning
