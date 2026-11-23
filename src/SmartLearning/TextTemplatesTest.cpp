#include "TextTemplates.h"

#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "gtest/gtest.h"

namespace McBopomofo::SmartLearning {
namespace {

std::vector<std::string> codes(const std::vector<TextTemplateEntry>& entries) {
  std::vector<std::string> result;
  for (const auto& e : entries) {
    result.push_back(e.code);
  }
  return result;
}

TEST(TextTemplatesTest, ParsesCodesAndTexts) {
  auto entries = ParseTextTemplates(
      "# a comment\taddr\tnot this\n"
      "addr\t100 Main Street\n"
      "  tel \t+1 555 0100\n"
      "no tab here\n"
      "\tno code\n"
      "empty\t\n"
      "\n"
      "em\tname@example.com\tand a tab\r\n");
  ASSERT_EQ(entries.size(), 3u);
  EXPECT_EQ(entries[0], (TextTemplateEntry{"addr", "100 Main Street"}));
  EXPECT_EQ(entries[1], (TextTemplateEntry{"tel", "+1 555 0100"}));
  // Only the first tab separates the code; CR LF ends a line.
  EXPECT_EQ(entries[2], (TextTemplateEntry{"em", "name@example.com\tand a tab"}));
}

TEST(TextTemplatesTest, CodeIsTrimmedOfFullWidthSpacesToo) {
  auto entries = ParseTextTemplates("\xE3\x80\x80sig\xE3\x80\x80\tBye\n");
  ASSERT_EQ(entries.size(), 1u);
  EXPECT_EQ(entries[0].code, "sig");
}

TEST(TextTemplatesTest, Unescapes) {
  auto entries = ParseTextTemplates(
      "a\tline one\\nline two\n"
      "b\tcol\\tcol\n"
      "c\tback\\\\slash\n"
      "d\tother \\x kept\n"
      "e\ttrailing\\\n");
  ASSERT_EQ(entries.size(), 5u);
  EXPECT_EQ(entries[0].text, "line one\nline two");
  EXPECT_EQ(entries[1].text, "col\tcol");
  EXPECT_EQ(entries[2].text, "back\\slash");
  EXPECT_EQ(entries[3].text, "other x kept");
  EXPECT_EQ(entries[4].text, "trailing\\");
}

TEST(TextTemplatesTest, MatchOrder) {
  std::vector<TextTemplateEntry> all = {
      {"mailbox", "box"},      {"xmail", "x"},     {"Mail", "exact"},
      {"note", "check MAIL"},  {"other", "none"},  {"mail2", "prefix"},
  };
  EXPECT_EQ(codes(MatchTextTemplates(all, "")), codes(all));
  EXPECT_EQ(codes(MatchTextTemplates(all, "mail")),
            (std::vector<std::string>{"Mail", "mailbox", "mail2", "xmail",
                                      "note"}));
  EXPECT_EQ(codes(MatchTextTemplates(all, "MAIL")),
            (std::vector<std::string>{"Mail", "mailbox", "mail2", "xmail",
                                      "note"}));
  EXPECT_TRUE(MatchTextTemplates(all, "zzz").empty());
}

TEST(TextTemplatesTest, TextContainingIgnoresCaseBeyondAscii) {
  std::vector<TextTemplateEntry> all = {{"g", "Hauptstraße 1"},
                                        {"h", "plain"}};
  EXPECT_EQ(codes(MatchTextTemplates(all, "PLAIN")),
            (std::vector<std::string>{"h"}));
  EXPECT_EQ(codes(MatchTextTemplates(all, "STRASSE")),
            (std::vector<std::string>{"g"}));
}

TEST(TextTemplatesTest, NumberCodeGivesQuickNumbers) {
  std::vector<TextTemplateEntry> all = {{"2026", "a template"}};
  std::tm tm{};
  tm.tm_year = 2026 - 1900;
  tm.tm_mon = 0;
  tm.tm_mday = 1;
  tm.tm_hour = 12;
  std::time_t now = std::mktime(&tm);
  auto candidates = TextTemplateCandidates(all, "10*15/6", now);
  ASSERT_FALSE(candidates.empty());
  EXPECT_EQ(candidates[0], (TextTemplateEntry{"", "25"}));
  for (const auto& c : candidates) {
    EXPECT_TRUE(c.code.empty());
  }
  // A code that is not a number lists the templates.
  EXPECT_EQ(codes(TextTemplateCandidates(all, "", now)),
            (std::vector<std::string>{"2026"}));
}

TEST(TextTemplatesTest, DisplayText) {
  EXPECT_EQ(TextTemplateDisplayText({"", "25"}), "25");
  EXPECT_EQ(TextTemplateDisplayText({"sig", "Best regards,\nJohn"}),
            "sig  Best regards,⏎John");
  std::string thirty(30, 'a');
  EXPECT_EQ(TextTemplateDisplayText({"x", thirty}), "x  " + thirty);
  EXPECT_EQ(TextTemplateDisplayText({"x", thirty + "b"}), "x  " + thirty + "…");
  // An emoji made of several code points counts as one character.
  std::string family = "\xF0\x9F\x91\xA8\xE2\x80\x8D\xF0\x9F\x91\xA9";
  std::string text;
  for (int i = 0; i < 30; ++i) {
    text += family;
  }
  EXPECT_EQ(TextTemplateDisplayText({"f", text}), "f  " + text);
  EXPECT_EQ(TextTemplateDisplayText({"f", text + "z"}), "f  " + text + "…");
}

TEST(TextTemplatesTest, NumberOperatorKeys) {
  EXPECT_TRUE(IsNumberOperatorKey("10", '*'));
  EXPECT_TRUE(IsNumberOperatorKey("(1", '('));
  EXPECT_TRUE(IsNumberOperatorKey("12", ':'));
  EXPECT_FALSE(IsNumberOperatorKey("10", '!'));
  // Not a number code yet: Shift and a key picks a candidate.
  EXPECT_FALSE(IsNumberOperatorKey("", '('));
  EXPECT_FALSE(IsNumberOperatorKey("addr", '*'));
}

class TextTemplateStoreTest : public ::testing::Test {
 protected:
  void SetUp() override {
    auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    folder_ = std::filesystem::temp_directory_path() /
              ("text-templates-" + std::to_string(stamp));
    std::filesystem::create_directories(folder_);
    path_ = folder_ / "text-templates.txt";
  }
  void TearDown() override {
    std::error_code ec;
    std::filesystem::remove_all(folder_, ec);
  }
  void write(const std::string& content) {
    std::ofstream out(path_, std::ios::binary | std::ios::trunc);
    out << content;
  }

  std::filesystem::path folder_;
  std::filesystem::path path_;
  double now_ = 100;
};

TEST_F(TextTemplateStoreTest, ReadsAgainOnlyAfterTheInterval) {
  TextTemplateStore store([this] { return now_; });
  store.setFile(path_, "");
  EXPECT_TRUE(store.entries().empty());
  write("a\tone\n");
  // Within the interval the file is not looked at.
  now_ += 1;
  EXPECT_TRUE(store.entries().empty());
  now_ += TextTemplateStore::kRecheckInterval;
  ASSERT_EQ(store.entries().size(), 1u);
  write("a\tone\nbb\ttwo\n");
  now_ += TextTemplateStore::kRecheckInterval;
  EXPECT_EQ(store.entries().size(), 2u);
  std::filesystem::remove(path_);
  now_ += TextTemplateStore::kRecheckInterval;
  EXPECT_TRUE(store.entries().empty());
}

TEST_F(TextTemplateStoreTest, SampleIsWrittenOnlyWhenMissing) {
  TextTemplateStore store([this] { return now_; });
  store.setFile(path_, "s\tsample\n");
  EXPECT_TRUE(store.entries().empty());
  store.createSampleFileIfNeeded();
  // Read at once, without waiting for the interval.
  ASSERT_EQ(store.entries().size(), 1u);
  EXPECT_EQ(store.entries()[0].text, "sample");

  write("mine\tkept\n");
  store.createSampleFileIfNeeded();
  now_ += TextTemplateStore::kRecheckInterval;
  ASSERT_EQ(store.entries().size(), 1u);
  EXPECT_EQ(store.entries()[0].code, "mine");
}

TEST_F(TextTemplateStoreTest, NoSampleForAFolderNotToBeWritten) {
  TextTemplateStore store([this] { return now_; });
  store.setFile(path_, "");
  store.createSampleFileIfNeeded();
  EXPECT_FALSE(std::filesystem::exists(path_));
}

}  // namespace
}  // namespace McBopomofo::SmartLearning
