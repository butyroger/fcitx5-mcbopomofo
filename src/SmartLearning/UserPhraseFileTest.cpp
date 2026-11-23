#include "UserPhraseFile.h"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace McBopomofo::SmartLearning {
namespace fs = std::filesystem;

namespace {

void writeFile(const fs::path& path, const std::string& content) {
  fs::create_directories(path.parent_path());
  std::ofstream out(path, std::ios::binary);
  out << content;
}

std::string readFile(const fs::path& path) {
  std::ifstream in(path, std::ios::binary);
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

class UserPhraseFileTest : public ::testing::Test {
 protected:
  void SetUp() override {
    auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    root_ = fs::temp_directory_path() /
            ("smartlearning-userphrase-" + std::to_string(stamp) + "-" +
             ::testing::UnitTest::GetInstance()->current_test_info()->name());
    fs::create_directories(root_);
    data_ = root_ / "data.txt";
    exclude_ = root_ / "exclude-phrases.txt";
  }
  void TearDown() override {
    std::error_code ec;
    fs::remove_all(root_, ec);
  }

  // A folder where the file should be cannot be read as a file, even by root.
  static void makeUnreadable(const fs::path& path) {
    fs::remove(path);
    fs::create_directories(path / "keep");
  }

  fs::path root_;
  fs::path data_;
  fs::path exclude_;
};

}  // namespace

// MARK: - learn

TEST_F(UserPhraseFileTest, LearnAppendsANewReadingToTheEnd) {
  writeFile(data_, "# header\n甲 ㄐㄧㄚˇ\n");
  writeFile(exclude_, "");
  EXPECT_TRUE(UserPhraseFile::learn(data_, exclude_, "乙丙", "ㄧˇ-ㄅㄧㄥˇ"));
  EXPECT_EQ(readFile(data_), "# header\n甲 ㄐㄧㄚˇ\n乙丙 ㄧˇ-ㄅㄧㄥˇ\n");
}

TEST_F(UserPhraseFileTest, LearnAddsALineBreakBeforeAppending) {
  writeFile(data_, "甲 ㄐㄧㄚˇ");
  writeFile(exclude_, "");
  EXPECT_TRUE(UserPhraseFile::learn(data_, exclude_, "乙丙", "ㄧˇ-ㄅㄧㄥˇ"));
  EXPECT_EQ(readFile(data_), "甲 ㄐㄧㄚˇ\n乙丙 ㄧˇ-ㄅㄧㄥˇ\n");
}

TEST_F(UserPhraseFileTest, LearnGoesBeforeTheFirstPhraseOfTheSameReading) {
  writeFile(data_,
            "# header\n"
            "甲 ㄐㄧㄚˇ\n"
            "丁戊 ㄉㄧㄥ-ㄨˋ\n"
            "丁午 ㄉㄧㄥ-ㄨˋ\n"
            "頂物 ㄉㄧㄥ-ㄨˋ\n"
            "乙 ㄧˇ\n");
  writeFile(exclude_, "");
  EXPECT_TRUE(UserPhraseFile::learn(data_, exclude_, "頂物", "ㄉㄧㄥ-ㄨˋ"));
  // The older line of the phrase is removed, the trailing line break kept.
  EXPECT_EQ(readFile(data_),
            "# header\n"
            "甲 ㄐㄧㄚˇ\n"
            "頂物 ㄉㄧㄥ-ㄨˋ\n"
            "丁戊 ㄉㄧㄥ-ㄨˋ\n"
            "丁午 ㄉㄧㄥ-ㄨˋ\n"
            "乙 ㄧˇ\n");
}

TEST_F(UserPhraseFileTest, LearnLeavesTheFileAloneWhenAlreadyFirst) {
  const std::string content = "丁戊 ㄉㄧㄥ-ㄨˋ\n頂物 ㄉㄧㄥ-ㄨˋ\n丁戊 ㄉㄧㄥ-ㄨˋ\n";
  writeFile(data_, content);
  writeFile(exclude_, "");
  auto before = fs::last_write_time(data_);
  EXPECT_FALSE(UserPhraseFile::learn(data_, exclude_, "丁戊", "ㄉㄧㄥ-ㄨˋ"));
  EXPECT_EQ(readFile(data_), content);
  EXPECT_EQ(fs::last_write_time(data_), before);
}

TEST_F(UserPhraseFileTest, LearnNeverBringsBackAnExcludedPhrase) {
  writeFile(data_, "甲 ㄐㄧㄚˇ\n");
  writeFile(exclude_, "# excluded\n  乙丙 ㄧˇ-ㄅㄧㄥˇ \t\n");
  EXPECT_FALSE(UserPhraseFile::learn(data_, exclude_, "乙丙", "ㄧˇ-ㄅㄧㄥˇ"));
  EXPECT_EQ(readFile(data_), "甲 ㄐㄧㄚˇ\n");
}

TEST_F(UserPhraseFileTest, LearnMatchesTrimmedLinesSplitOnSingleSpaces) {
  // Lines are trimmed at both ends (also of ideographic spaces) before being
  // split on single spaces into exactly two columns. A line with two spaces
  // between the columns, a tab, or a comment does not count.
  writeFile(data_,
            "#丁戊 ㄉㄧㄥ-ㄨˋ\n"
            "丁午  ㄉㄧㄥ-ㄨˋ\n"
            "丁武\tㄉㄧㄥ-ㄨˋ\n"
            "  \xe3\x80\x80丁戊 ㄉㄧㄥ-ㄨˋ \r\n"
            "頂物 ㄉㄧㄥ-ㄨˋ\n");
  writeFile(exclude_, "");
  EXPECT_TRUE(UserPhraseFile::learn(data_, exclude_, "頂物", "ㄉㄧㄥ-ㄨˋ"));
  EXPECT_EQ(readFile(data_),
            "#丁戊 ㄉㄧㄥ-ㄨˋ\n"
            "丁午  ㄉㄧㄥ-ㄨˋ\n"
            "丁武\tㄉㄧㄥ-ㄨˋ\n"
            "頂物 ㄉㄧㄥ-ㄨˋ\n"
            "  \xe3\x80\x80丁戊 ㄉㄧㄥ-ㄨˋ \r\n");

  // The indented line is the first of the reading, so it is already first.
  writeFile(data_, " 頂物 ㄉㄧㄥ-ㄨˋ\n丁戊 ㄉㄧㄥ-ㄨˋ\n");
  EXPECT_FALSE(UserPhraseFile::learn(data_, exclude_, "頂物", "ㄉㄧㄥ-ㄨˋ"));
}

TEST_F(UserPhraseFileTest, LearnNeedsTheDataFileAndAPlainPhrase) {
  writeFile(exclude_, "");
  EXPECT_FALSE(UserPhraseFile::learn(data_, exclude_, "甲", "ㄐㄧㄚˇ"));
  EXPECT_FALSE(fs::exists(data_));

  writeFile(data_, "");
  EXPECT_FALSE(UserPhraseFile::learn(data_, exclude_, "甲 乙", "ㄐㄧㄚˇ-ㄧˇ"));
  EXPECT_FALSE(UserPhraseFile::learn(data_, exclude_, "甲", "ㄐㄧㄚˇ ㄧˇ"));
  EXPECT_FALSE(UserPhraseFile::learn(data_, exclude_, "甲\n", "ㄐㄧㄚˇ"));
  EXPECT_FALSE(UserPhraseFile::learn(data_, exclude_, "", "ㄐㄧㄚˇ"));
  EXPECT_EQ(readFile(data_), "");

  // A missing exclude file excludes nothing.
  fs::remove(exclude_);
  EXPECT_TRUE(UserPhraseFile::learn(data_, exclude_, "甲", "ㄐㄧㄚˇ"));
  EXPECT_EQ(readFile(data_), "甲 ㄐㄧㄚˇ\n");
}

TEST_F(UserPhraseFileTest, LearnNeverWritesAfterAFailedRead) {
  writeFile(exclude_, "");
  makeUnreadable(data_);
  EXPECT_FALSE(UserPhraseFile::learn(data_, exclude_, "乙", "ㄧˇ"));
  EXPECT_TRUE(fs::is_directory(data_ / "keep"));

  // An exclude file that cannot be read might hold the phrase.
  fs::remove_all(data_);
  writeFile(data_, "甲 ㄐㄧㄚˇ\n");
  makeUnreadable(exclude_);
  EXPECT_FALSE(UserPhraseFile::learn(data_, exclude_, "乙", "ㄧˇ"));
  EXPECT_EQ(readFile(data_), "甲 ㄐㄧㄚˇ\n");
}

// MARK: - learn, several phrases

TEST_F(UserPhraseFileTest, LearnSeveralWritesOnceInOrder) {
  writeFile(data_, "# header\n丁戊 ㄉㄧㄥ-ㄨˋ\n");
  writeFile(exclude_, "己庚 ㄐㄧˇ-ㄍㄥ\n");
  using Phrases = std::vector<std::pair<std::string, std::string>>;
  Phrases learned = UserPhraseFile::learn(
      data_, exclude_,
      Phrases{{"乙丙", "ㄧˇ-ㄅㄧㄥˇ"},
              {"己庚", "ㄐㄧˇ-ㄍㄥ"},      // excluded
              {"丁午", "ㄉㄧㄥ-ㄨˋ"},      // before 丁戊
              {"丁戊", "ㄉㄧㄥ-ㄨˋ"},      // was first, now second: moves back
              {"有 空", "ㄧㄡˇ-ㄎㄨㄥ"}});  // not a column
  EXPECT_EQ(learned, (Phrases{{"乙丙", "ㄧˇ-ㄅㄧㄥˇ"},
                              {"丁午", "ㄉㄧㄥ-ㄨˋ"},
                              {"丁戊", "ㄉㄧㄥ-ㄨˋ"}}));
  EXPECT_EQ(readFile(data_),
            "# header\n丁戊 ㄉㄧㄥ-ㄨˋ\n丁午 ㄉㄧㄥ-ㄨˋ\n乙丙 ㄧˇ-ㄅㄧㄥˇ\n");
}

TEST_F(UserPhraseFileTest, LearnSeveralWritesNothingWhenNothingIsLearned) {
  writeFile(data_, "乙丙 ㄧˇ-ㄅㄧㄥˇ\n");
  writeFile(exclude_, "");
  auto before = fs::last_write_time(data_);
  using Phrases = std::vector<std::pair<std::string, std::string>>;
  EXPECT_TRUE(UserPhraseFile::learn(data_, exclude_,
                                    Phrases{{"乙丙", "ㄧˇ-ㄅㄧㄥˇ"}})
                  .empty());
  EXPECT_TRUE(UserPhraseFile::learn(data_, exclude_, Phrases{}).empty());
  EXPECT_EQ(fs::last_write_time(data_), before);
  EXPECT_EQ(readFile(data_), "乙丙 ㄧˇ-ㄅㄧㄥˇ\n");
}

TEST_F(UserPhraseFileTest, LearnSeveralNeedsTheDataFile) {
  writeFile(exclude_, "");
  using Phrases = std::vector<std::pair<std::string, std::string>>;
  EXPECT_TRUE(UserPhraseFile::learn(data_, exclude_,
                                    Phrases{{"乙丙", "ㄧˇ-ㄅㄧㄥˇ"}})
                  .empty());
  EXPECT_FALSE(fs::exists(data_));

  writeFile(data_, "");
  makeUnreadable(exclude_);
  EXPECT_TRUE(UserPhraseFile::learn(data_, exclude_,
                                    Phrases{{"乙丙", "ㄧˇ-ㄅㄧㄥˇ"}})
                  .empty());
  EXPECT_EQ(readFile(data_), "");
}

TEST_F(UserPhraseFileTest, FitsInColumn) {
  EXPECT_TRUE(UserPhraseFile::fitsInColumn("許氏"));
  EXPECT_FALSE(UserPhraseFile::fitsInColumn(""));
  EXPECT_FALSE(UserPhraseFile::fitsInColumn("許 氏"));
  EXPECT_FALSE(UserPhraseFile::fitsInColumn("許\t氏"));
  EXPECT_FALSE(UserPhraseFile::fitsInColumn("許\n"));
  EXPECT_FALSE(UserPhraseFile::fitsInColumn("許\u3000氏"));
  EXPECT_FALSE(UserPhraseFile::fitsInColumn("許\u00A0氏"));
}

// MARK: - write

TEST_F(UserPhraseFileTest, WriteRemovesTheExclusionAndAppends) {
  writeFile(data_, "丁戊 ㄉㄧㄥ-ㄨˋ\n");
  writeFile(exclude_, "# excluded\n 頂物 ㄉㄧㄥ-ㄨˋ\n乙 ㄧˇ\n");
  EXPECT_TRUE(UserPhraseFile::write(data_, exclude_, "頂物", "ㄉㄧㄥ-ㄨˋ"));
  // Appended to the end, not put first.
  EXPECT_EQ(readFile(data_), "丁戊 ㄉㄧㄥ-ㄨˋ\n頂物 ㄉㄧㄥ-ㄨˋ\n");
  EXPECT_EQ(readFile(exclude_), "# excluded\n乙 ㄧˇ\n");
}

TEST_F(UserPhraseFileTest, WriteDoesNotDuplicateAPhrase) {
  writeFile(data_, "  頂物 ㄉㄧㄥ-ㄨˋ\n");
  writeFile(exclude_, "頂物 ㄉㄧㄥ-ㄨˋ\n");
  EXPECT_FALSE(UserPhraseFile::write(data_, exclude_, "頂物", "ㄉㄧㄥ-ㄨˋ"));
  EXPECT_EQ(readFile(data_), "  頂物 ㄉㄧㄥ-ㄨˋ\n");
  // The exclusion is removed even so.
  EXPECT_EQ(readFile(exclude_), "");
}

TEST_F(UserPhraseFileTest, WriteNeedsTheDataFile) {
  writeFile(exclude_, "頂物 ㄉㄧㄥ-ㄨˋ\n");
  EXPECT_FALSE(UserPhraseFile::write(data_, exclude_, "頂物", "ㄉㄧㄥ-ㄨˋ"));
  EXPECT_FALSE(fs::exists(data_));
  EXPECT_EQ(readFile(exclude_), "頂物 ㄉㄧㄥ-ㄨˋ\n");
}

TEST_F(UserPhraseFileTest, WriteNeverWritesAfterAFailedRead) {
  writeFile(exclude_, "乙 ㄧˇ\n");
  makeUnreadable(data_);
  EXPECT_FALSE(UserPhraseFile::write(data_, exclude_, "乙", "ㄧˇ"));
  EXPECT_TRUE(fs::is_directory(data_ / "keep"));
  // Nothing is written at all, not even the exclusion removed.
  EXPECT_EQ(readFile(exclude_), "乙 ㄧˇ\n");

  // An exclude file that cannot be read is left as it is.
  fs::remove_all(data_);
  writeFile(data_, "甲 ㄐㄧㄚˇ\n");
  makeUnreadable(exclude_);
  EXPECT_TRUE(UserPhraseFile::write(data_, exclude_, "乙", "ㄧˇ"));
  EXPECT_TRUE(fs::is_directory(exclude_ / "keep"));
  EXPECT_EQ(readFile(data_), "甲 ㄐㄧㄚˇ\n乙 ㄧˇ\n");
}

// MARK: - remove

TEST_F(UserPhraseFileTest, RemoveTakesThePhraseOutAndExcludesIt) {
  writeFile(data_, "# header\n頂物 ㄉㄧㄥ-ㄨˋ\n丁戊 ㄉㄧㄥ-ㄨˋ\n\t頂物 ㄉㄧㄥ-ㄨˋ\n");
  writeFile(exclude_, "# excluded");
  EXPECT_TRUE(UserPhraseFile::remove(data_, exclude_, "頂物", "ㄉㄧㄥ-ㄨˋ"));
  EXPECT_EQ(readFile(data_), "# header\n丁戊 ㄉㄧㄥ-ㄨˋ\n");
  EXPECT_EQ(readFile(exclude_), "# excluded\n頂物 ㄉㄧㄥ-ㄨˋ\n");
}

TEST_F(UserPhraseFileTest, RemoveDoesNotDuplicateAnExclusion) {
  writeFile(data_, "頂物 ㄉㄧㄥ-ㄨˋ\n");
  writeFile(exclude_, "頂物 ㄉㄧㄥ-ㄨˋ\n");
  EXPECT_FALSE(UserPhraseFile::remove(data_, exclude_, "頂物", "ㄉㄧㄥ-ㄨˋ"));
  EXPECT_EQ(readFile(data_), "");
  EXPECT_EQ(readFile(exclude_), "頂物 ㄉㄧㄥ-ㄨˋ\n");
}

TEST_F(UserPhraseFileTest, RemoveNeedsTheExcludeFile) {
  writeFile(data_, "頂物 ㄉㄧㄥ-ㄨˋ\n");
  EXPECT_FALSE(UserPhraseFile::remove(data_, exclude_, "頂物", "ㄉㄧㄥ-ㄨˋ"));
  EXPECT_FALSE(fs::exists(exclude_));
  EXPECT_EQ(readFile(data_), "頂物 ㄉㄧㄥ-ㄨˋ\n");
}

TEST_F(UserPhraseFileTest, RemoveNeverWritesAfterAFailedRead) {
  writeFile(data_, "頂物 ㄉㄧㄥ-ㄨˋ\n");
  makeUnreadable(exclude_);
  EXPECT_FALSE(UserPhraseFile::remove(data_, exclude_, "頂物", "ㄉㄧㄥ-ㄨˋ"));
  EXPECT_TRUE(fs::is_directory(exclude_ / "keep"));
  // Nothing is written at all, not even the phrase taken out of data.txt.
  EXPECT_EQ(readFile(data_), "頂物 ㄉㄧㄥ-ㄨˋ\n");

  // A data file that cannot be read is left as it is; the phrase is still
  // excluded.
  fs::remove_all(exclude_);
  writeFile(exclude_, "乙 ㄧˇ\n");
  makeUnreadable(data_);
  EXPECT_TRUE(UserPhraseFile::remove(data_, exclude_, "頂物", "ㄉㄧㄥ-ㄨˋ"));
  EXPECT_TRUE(fs::is_directory(data_ / "keep"));
  EXPECT_EQ(readFile(exclude_), "乙 ㄧˇ\n頂物 ㄉㄧㄥ-ㄨˋ\n");
}

}  // namespace McBopomofo::SmartLearning
