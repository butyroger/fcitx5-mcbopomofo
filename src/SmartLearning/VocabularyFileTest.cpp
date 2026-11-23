#include "VocabularyFile.h"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace McBopomofo::SmartLearning::VocabularyFile {
namespace fs = std::filesystem;

namespace {

void writeFile(const fs::path& path, const std::string& content) {
  std::ofstream out(path, std::ios::binary);
  out << content;
}

std::string readFile(const fs::path& path) {
  std::ifstream in(path, std::ios::binary);
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

class VocabularyFileTest : public ::testing::Test {
 protected:
  void SetUp() override {
    auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    root_ = fs::temp_directory_path() /
            ("smartlearning-vocabfile-" + std::to_string(stamp) + "-" +
             ::testing::UnitTest::GetInstance()->current_test_info()->name());
    fs::create_directories(root_);
    path_ = root_ / "data.txt";
  }
  void TearDown() override {
    std::error_code ec;
    fs::remove_all(root_, ec);
  }

  fs::path root_;
  fs::path path_;
};

}  // namespace

TEST_F(VocabularyFileTest, ReplaceKeepsCommentsAndOrder) {
  writeFile(path_, "# comment\n甲 ㄅ\n\n乙 ㄆ\n丙 ㄇ\n");
  EXPECT_TRUE(replaceOrAppendLine(path_, "乙乙 ㄆ", "乙 ㄆ"));
  EXPECT_EQ(readFile(path_), "# comment\n甲 ㄅ\n\n乙乙 ㄆ\n丙 ㄇ\n");
}

TEST_F(VocabularyFileTest, ReplaceOnlyTheFirstIdenticalLine) {
  writeFile(path_, "甲 ㄅ\n甲 ㄅ\n");
  EXPECT_TRUE(replaceOrAppendLine(path_, "乙 ㄆ", "甲 ㄅ"));
  EXPECT_EQ(readFile(path_), "乙 ㄆ\n甲 ㄅ\n");
}

TEST_F(VocabularyFileTest, OldLineNotFoundAppends) {
  writeFile(path_, "甲 ㄅ\n");
  EXPECT_TRUE(replaceOrAppendLine(path_, "乙 ㄆ", "沒有 ㄈ"));
  EXPECT_EQ(readFile(path_), "甲 ㄅ\n乙 ㄆ\n");
}

TEST_F(VocabularyFileTest, OldLineMatchIsExact) {
  writeFile(path_, "甲 ㄅ \n");
  EXPECT_TRUE(replaceOrAppendLine(path_, "乙 ㄆ", "甲 ㄅ"));
  EXPECT_EQ(readFile(path_), "甲 ㄅ \n乙 ㄆ\n");
}

TEST_F(VocabularyFileTest, AppendAddsMissingFinalNewline) {
  writeFile(path_, "甲 ㄅ");
  EXPECT_TRUE(replaceOrAppendLine(path_, "乙 ㄆ", std::nullopt));
  EXPECT_EQ(readFile(path_), "甲 ㄅ\n乙 ㄆ\n");
}

TEST_F(VocabularyFileTest, AppendToEmptyFile) {
  writeFile(path_, "");
  EXPECT_TRUE(replaceOrAppendLine(path_, "乙 ㄆ", std::nullopt));
  EXPECT_EQ(readFile(path_), "乙 ㄆ\n");
}

TEST_F(VocabularyFileTest, ReplaceKeepsCarriageReturnsOfOtherLines) {
  writeFile(path_, "甲 ㄅ\r\n乙 ㄆ\r\n");
  EXPECT_TRUE(replaceOrAppendLine(path_, "丙 ㄇ", "乙 ㄆ\r"));
  EXPECT_EQ(readFile(path_), "甲 ㄅ\r\n丙 ㄇ\n");
}

TEST_F(VocabularyFileTest, MissingFileIsNotWritten) {
  EXPECT_FALSE(replaceOrAppendLine(path_, "乙 ㄆ", std::nullopt));
  EXPECT_FALSE(fs::exists(path_));
  EXPECT_FALSE(removeLines(path_, {"乙 ㄆ"}));
  EXPECT_FALSE(fs::exists(path_));
}

TEST_F(VocabularyFileTest, UnreadableFileIsNotWritten) {
  fs::create_directories(path_ / "keep");
  EXPECT_FALSE(replaceOrAppendLine(path_, "乙 ㄆ", std::nullopt));
  EXPECT_FALSE(removeLines(path_, {"乙 ㄆ"}));
  EXPECT_TRUE(fs::is_directory(path_));
}

TEST_F(VocabularyFileTest, RemoveKeepsCommentsAndOrder) {
  writeFile(path_, "# comment\n甲 ㄅ\n\n乙 ㄆ\n丙 ㄇ\n");
  EXPECT_TRUE(removeLines(path_, {"乙 ㄆ"}));
  EXPECT_EQ(readFile(path_), "# comment\n甲 ㄅ\n\n丙 ㄇ\n");
}

TEST_F(VocabularyFileTest, RemoveOnlyOneOfTwoIdenticalLines) {
  writeFile(path_, "甲 ㄅ\n乙 ㄆ\n甲 ㄅ\n");
  EXPECT_TRUE(removeLines(path_, {"甲 ㄅ"}));
  EXPECT_EQ(readFile(path_), "乙 ㄆ\n甲 ㄅ\n");
}

TEST_F(VocabularyFileTest, RemoveTwiceRemovesBothIdenticalLines) {
  writeFile(path_, "甲 ㄅ\n乙 ㄆ\n甲 ㄅ\n");
  EXPECT_TRUE(removeLines(path_, {"甲 ㄅ", "甲 ㄅ"}));
  EXPECT_EQ(readFile(path_), "乙 ㄆ\n");
}

TEST_F(VocabularyFileTest, RemoveLineThatIsNotThereChangesNothing) {
  writeFile(path_, "甲 ㄅ\n");
  EXPECT_TRUE(removeLines(path_, {"沒有 ㄈ"}));
  EXPECT_EQ(readFile(path_), "甲 ㄅ\n");
}

}  // namespace McBopomofo::SmartLearning::VocabularyFile
