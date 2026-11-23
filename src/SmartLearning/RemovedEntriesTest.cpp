#include "RemovedEntries.h"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

namespace McBopomofo::SmartLearning {
namespace fs = std::filesystem;

namespace {

void writeFile(const fs::path& path, const std::string& content) {
  fs::create_directories(path.parent_path());
  std::ofstream out(path, std::ios::binary);
  out << content;
}

class RemovedEntriesTest : public ::testing::Test {
 protected:
  void SetUp() override {
    auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    root_ = fs::temp_directory_path() /
            ("smartlearning-removed-" + std::to_string(stamp) + "-" +
             ::testing::UnitTest::GetInstance()->current_test_info()->name());
    learning_ = root_ / "learning";
    fs::create_directories(learning_);
  }
  void TearDown() override {
    std::error_code ec;
    fs::remove_all(root_, ec);
  }

  fs::path removedFile() const { return learning_ / "removed.txt"; }

  fs::path root_;
  fs::path learning_;
  double now_ = 1000;
};

}  // namespace

TEST_F(RemovedEntriesTest, NoFolderOrFileMeansNothingRemoved) {
  RemovedEntries entries([this] { return now_; });
  EXPECT_EQ(entries.removedTime(RemovedEntries::kEnglish, "hello"), 0);
  entries.setLearningFolder(learning_);
  EXPECT_EQ(entries.removedTime(RemovedEntries::kTemporary, "a b"), 0);
}

TEST_F(RemovedEntriesTest, ParsesExactlyThreeColumnsWithANumericTime) {
  writeFile(removedFile(),
            "english\thello\t1700000000\n"
            "temporary\t許氏 ㄒㄩˇ-ㄕˋ\t1700000100.5\n"
            "sentence\ttoo\tmany\t1700000200\n"
            "english\tshort\n"
            "english\tnotime\tsoon\n"
            "english\tblank\t\n"
            "english\tspaced\t 12\n"
            "\n"
            "english\tsigned\t+42\n");
  RemovedEntries entries([this] { return now_; });
  entries.setLearningFolder(learning_);
  EXPECT_EQ(entries.removedTime("english", "hello"), 1700000000);
  EXPECT_EQ(entries.removedTime("temporary", "許氏 ㄒㄩˇ-ㄕˋ"), 1700000100.5);
  EXPECT_EQ(entries.removedTime("sentence", "too"), 0);
  EXPECT_EQ(entries.removedTime("english", "short"), 0);
  EXPECT_EQ(entries.removedTime("english", "notime"), 0);
  EXPECT_EQ(entries.removedTime("english", "blank"), 0);
  EXPECT_EQ(entries.removedTime("english", "spaced"), 0);
  EXPECT_EQ(entries.removedTime("english", "signed"), 42);
  // The kind must match too.
  EXPECT_EQ(entries.removedTime("sentence", "hello"), 0);
}

TEST_F(RemovedEntriesTest, KeepsEmptyColumns) {
  writeFile(removedFile(), "english\t\t5\n\tkey\t6\n");
  RemovedEntries entries([this] { return now_; });
  entries.setLearningFolder(learning_);
  EXPECT_EQ(entries.removedTime("english", ""), 5);
  EXPECT_EQ(entries.removedTime("", "key"), 6);
}

TEST_F(RemovedEntriesTest, RereadsAfterSixtySeconds) {
  writeFile(removedFile(), "english\thello\t10\n");
  RemovedEntries entries([this] { return now_; });
  entries.setLearningFolder(learning_);
  EXPECT_EQ(entries.removedTime("english", "hello"), 10);

  writeFile(removedFile(), "english\thello\t20\nenglish\tworld\t30\n");
  now_ += 59;
  EXPECT_EQ(entries.removedTime("english", "hello"), 10);
  EXPECT_EQ(entries.removedTime("english", "world"), 0);
  now_ += 1;
  EXPECT_EQ(entries.removedTime("english", "hello"), 20);
  EXPECT_EQ(entries.removedTime("english", "world"), 30);
}

TEST_F(RemovedEntriesTest, SwitchingFoldersRereadsAndNeverWrites) {
  writeFile(removedFile(), "english\thello\t10\n");
  const fs::path other = root_ / "other" / "learning";
  writeFile(other / "removed.txt", "english\thello\t99\n");
  RemovedEntries entries([this] { return now_; });
  entries.setLearningFolder(learning_);
  EXPECT_EQ(entries.removedTime("english", "hello"), 10);
  entries.setLearningFolder(other);
  EXPECT_EQ(entries.removedTime("english", "hello"), 99);

  const fs::path missing = root_ / "missing" / "learning";
  entries.setLearningFolder(missing);
  EXPECT_EQ(entries.removedTime("english", "hello"), 0);
  EXPECT_FALSE(fs::exists(missing));
}

}  // namespace McBopomofo::SmartLearning
