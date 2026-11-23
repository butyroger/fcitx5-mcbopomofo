#include "BopomofoKeys.h"

#include <set>

#include "gtest/gtest.h"

namespace McBopomofo::SmartLearning {

using Formosa::Mandarin::BopomofoKeyboardLayout;
using Formosa::Mandarin::BopomofoReadingBuffer;

namespace {

bool Has(const std::string& r) {
  static const std::set<std::string> s = {"ㄏㄠˇ", "ㄉㄤˇ", "ㄋㄧ"};
  return s.count(r) > 0;
}
bool IsWord(const std::string& w) {
  static const std::set<std::string> s = {"hello", "PDF"};
  return s.count(w) > 0;
}
bool IsWordOrPrefix(const std::string& w) {
  return std::string("helpful").rfind(w, 0) == 0 || IsWord(w);
}

BopomofoReadingBuffer Typed(const BopomofoKeyboardLayout* l,
                            const std::string& keys) {
  BopomofoReadingBuffer b(l);
  for (char k : keys) b.combineKey(k);
  return b;
}

}  // namespace

TEST(BopomofoKeysTest, HelloHaoStandard) {
  auto r = PlanTrailingEnglishToChinese(
      "hellocl", '3', BopomofoKeyboardLayout::StandardLayout(), Has, IsWord,
      IsWordOrPrefix);
  ASSERT_TRUE(r);
  EXPECT_EQ(r->lettersToDelete, 2u);
  EXPECT_EQ(r->reading, "ㄏㄠˇ");
}

TEST(BopomofoKeysTest, HelloHaoHsu) {
  auto r = PlanTrailingEnglishToChinese(
      "hellohw", 'f', BopomofoKeyboardLayout::HsuLayout(), Has, IsWord,
      IsWordOrPrefix);
  ASSERT_TRUE(r);
  EXPECT_EQ(r->lettersToDelete, 2u);
  EXPECT_EQ(r->reading, "ㄏㄠˇ");
}

TEST(BopomofoKeysTest, PdfHaoStandard) {
  // The standard keys of ㄉㄤ are "2;", which are not letters and never form a
  // run, so an acronym is checked with letter-only keys: ㄏㄠ = "cl".
  auto r = PlanTrailingEnglishToChinese(
      "PDFcl", '3', BopomofoKeyboardLayout::StandardLayout(), Has, IsWord,
      IsWordOrPrefix);
  ASSERT_TRUE(r);
  EXPECT_EQ(r->reading, "ㄏㄠˇ");
  EXPECT_EQ(r->lettersToDelete, 2u);
}

TEST(BopomofoKeysTest, PdfDangHsu) {
  // ㄉ = "d", ㄤ = "k", tone ˇ = "f".
  auto r = PlanTrailingEnglishToChinese(
      "PDFdk", 'f', BopomofoKeyboardLayout::HsuLayout(), Has, IsWord,
      IsWordOrPrefix);
  ASSERT_TRUE(r);
  EXPECT_EQ(r->reading, "ㄉㄤˇ");
  EXPECT_EQ(r->lettersToDelete, 2u);
}

TEST(BopomofoKeysTest, HelpfulPrefixNotConverted) {
  EXPECT_FALSE(PlanTrailingEnglishToChinese(
      "help", 'f', BopomofoKeyboardLayout::HsuLayout(), Has, IsWord,
      IsWordOrPrefix));
}

TEST(BopomofoKeysTest, NonCanonicalKeysSkipped) {
  auto r = PlanTrailingEnglishToChinese(
      "hellone", ' ', BopomofoKeyboardLayout::HsuLayout(), Has, IsWord,
      IsWordOrPrefix);
  ASSERT_TRUE(r);
  EXPECT_EQ(r->lettersToDelete, 2u);
  EXPECT_EQ(r->reading, "ㄋㄧ");
}

TEST(BopomofoKeysTest, WholeWordWithSpaceNotConverted) {
  EXPECT_FALSE(PlanTrailingEnglishToChinese(
      "hello", ' ', BopomofoKeyboardLayout::HsuLayout(), Has, IsWord,
      IsWordOrPrefix));
}

TEST(BopomofoKeysTest, ComponentReplaced) {
  auto* l = BopomofoKeyboardLayout::StandardLayout();
  EXPECT_TRUE(KeyReplacesComponent(Typed(l, "ap"), 'p'));
  EXPECT_FALSE(KeyReplacesComponent(Typed(l, "a"), 'j'));
  EXPECT_FALSE(KeyReplacesComponent(Typed(l, "a"), '3'));
}

TEST(BopomofoKeysTest, RawKeysSameAndFallback) {
  auto* l = BopomofoKeyboardLayout::StandardLayout();
  auto b = Typed(l, "cl");
  EXPECT_EQ(RawKeysOfCurrentReading(b, "cl"), "cl");
  EXPECT_EQ(RawKeysOfCurrentReading(b, "clx"), "cl");  // canonical
  EXPECT_EQ(RawKeysOfCurrentReading(BopomofoReadingBuffer(l), "cl"), "");
}

}  // namespace McBopomofo::SmartLearning
