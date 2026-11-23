#include "RemovedEntries.h"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

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

TEST_F(RemovedEntriesTest, AddWritesSortedLinesInWholeSeconds) {
  writeFile(removedFile(),
            "sentence\t明天見面再聊吧\t1700000000.75\n"
            "english\tbroken\n"
            "temporary\t甲乙 ㄐㄧㄚˇ-ㄧˇ\t1700000100\n");
  RemovedEntries entries([this] { return now_; });
  entries.setLearningFolder(learning_);
  ASSERT_TRUE(entries.add(RemovedEntries::kEnglish, "zyxwv", 1700000200.9));
  // Sorted by line; lines that cannot be read are dropped.
  EXPECT_EQ(readFile(removedFile()),
            "english\tzyxwv\t1700000200\n"
            "sentence\t明天見面再聊吧\t1700000000\n"
            "temporary\t甲乙 ㄐㄧㄚˇ-ㄧˇ\t1700000100\n");
  EXPECT_EQ(entries.removedTime(RemovedEntries::kEnglish, "zyxwv"),
            1700000200.9);

  // Adding again replaces the time.
  ASSERT_TRUE(entries.add(RemovedEntries::kEnglish, "zyxwv", 1700000300));
  EXPECT_EQ(readFile(removedFile()),
            "english\tzyxwv\t1700000300\n"
            "sentence\t明天見面再聊吧\t1700000000\n"
            "temporary\t甲乙 ㄐㄧㄚˇ-ㄧˇ\t1700000100\n");
}

TEST_F(RemovedEntriesTest, AddAllWritesAllOrNothing) {
  writeFile(removedFile(), "english\told\t1700000000\n");
  RemovedEntries entries([this] { return now_; });
  entries.setLearningFolder(learning_);
  // One key that breaks the line format: nothing is written.
  EXPECT_FALSE(entries.addAll(RemovedEntries::kEnglish, {"apple", "bad\tword"},
                              1700000100));
  EXPECT_EQ(readFile(removedFile()), "english\told\t1700000000\n");
  // No keys: nothing to write.
  EXPECT_TRUE(entries.addAll(RemovedEntries::kEnglish, {}, 1700000100));
  EXPECT_EQ(readFile(removedFile()), "english\told\t1700000000\n");
  ASSERT_TRUE(entries.addAll(RemovedEntries::kEnglish, {"kiwi", "apple"},
                             1700000100));
  EXPECT_EQ(readFile(removedFile()),
            "english\tapple\t1700000100\n"
            "english\tkiwi\t1700000100\n"
            "english\told\t1700000000\n");
  EXPECT_EQ(entries.removedTime(RemovedEntries::kEnglish, "kiwi"), 1700000100);
}

TEST_F(RemovedEntriesTest, AddAndRemoveReadTheFileFirst) {
  RemovedEntries entries([this] { return now_; });
  entries.setLearningFolder(learning_);
  EXPECT_EQ(entries.removedTime(RemovedEntries::kEnglish, "hello"), 0);
  // Written by another program within the reload interval.
  writeFile(removedFile(), "english\thello\t10\n");
  ASSERT_TRUE(entries.add(RemovedEntries::kSentence, "謝謝您的耐心等候", 20));
  EXPECT_EQ(readFile(removedFile()),
            "english\thello\t10\nsentence\t謝謝您的耐心等候\t20\n");
  EXPECT_EQ(entries.removedTime(RemovedEntries::kEnglish, "hello"), 10);

  writeFile(removedFile(), "english\thello\t10\nenglish\tworld\t30\n");
  ASSERT_TRUE(entries.remove(RemovedEntries::kEnglish, "hello"));
  EXPECT_EQ(readFile(removedFile()), "english\tworld\t30\n");
  EXPECT_EQ(entries.removedTime(RemovedEntries::kEnglish, "hello"), 0);
  EXPECT_EQ(entries.removedTime(RemovedEntries::kEnglish, "world"), 30);
}

TEST_F(RemovedEntriesTest, RemovingAnAbsentEntryWritesNothing) {
  RemovedEntries entries([this] { return now_; });
  entries.setLearningFolder(learning_);
  EXPECT_TRUE(entries.remove(RemovedEntries::kEnglish, "hello"));
  EXPECT_FALSE(fs::exists(removedFile()));
  writeFile(removedFile(), "english\tworld\t30\n");
  EXPECT_TRUE(entries.remove(RemovedEntries::kEnglish, "hello"));
  EXPECT_EQ(readFile(removedFile()), "english\tworld\t30\n");
}

TEST_F(RemovedEntriesTest, AddCreatesTheFileButNotTheUserDataFolder) {
  RemovedEntries entries([this] { return now_; });
  // The learning folder may be created, inside an existing user data folder.
  fs::remove_all(learning_);
  entries.setLearningFolder(learning_);
  ASSERT_TRUE(entries.add(RemovedEntries::kEnglish, "hello", 10));
  EXPECT_EQ(readFile(removedFile()), "english\thello\t10\n");

  // A missing user data folder may be a sync folder that has not arrived.
  const fs::path missing = root_ / "missing" / "learning";
  entries.setLearningFolder(missing);
  EXPECT_FALSE(entries.add(RemovedEntries::kEnglish, "hello", 10));
  EXPECT_FALSE(fs::exists(root_ / "missing"));
}

TEST_F(RemovedEntriesTest, UnreadableFileIsNeverWritten) {
  fs::create_directories(removedFile());  // a folder where the file should be
  RemovedEntries entries([this] { return now_; });
  entries.setLearningFolder(learning_);
  EXPECT_FALSE(entries.add(RemovedEntries::kEnglish, "hello", 10));
  EXPECT_FALSE(entries.remove(RemovedEntries::kEnglish, "hello"));
  EXPECT_TRUE(fs::is_directory(removedFile()));

  RemovedEntries noFolder([this] { return now_; });
  EXPECT_FALSE(noFolder.add(RemovedEntries::kEnglish, "hello", 10));
}

TEST_F(RemovedEntriesTest, KeysThatBreakTheLineFormatAreRefused) {
  RemovedEntries entries([this] { return now_; });
  entries.setLearningFolder(learning_);
  for (const char* key : {"a\tb", "a\nb", "a\rb", "a\vb", "a\fb", "a\u0085b",
                          "a\u2028b", "a\u2029b", "ab\r"}) {
    EXPECT_FALSE(entries.add(RemovedEntries::kEnglish, key, 10)) << key;
    EXPECT_FALSE(entries.add(key, "ab", 10)) << key;
  }
  EXPECT_FALSE(fs::exists(removedFile()));

  writeFile(removedFile(), "english\tab\t10\n");
  EXPECT_FALSE(entries.remove(RemovedEntries::kEnglish, "ab\r"));
  EXPECT_FALSE(entries.remove("english\n", "ab"));
  EXPECT_EQ(readFile(removedFile()), "english\tab\t10\n");
  // Other characters are fine.
  EXPECT_TRUE(entries.add(RemovedEntries::kEnglish, "a b\u00A0c", 20));
}

TEST_F(RemovedEntriesTest, ReloadIfChangedLooksAtTheStamp) {
  RemovedEntries entries([this] { return now_; });
  entries.setLearningFolder(learning_);
  EXPECT_EQ(entries.removedTime(RemovedEntries::kEnglish, "hello"), 0);
  EXPECT_FALSE(entries.reloadIfChanged());
  writeFile(removedFile(), "english\thello\t10\n");
  EXPECT_TRUE(entries.reloadIfChanged());
  EXPECT_EQ(entries.removedTime(RemovedEntries::kEnglish, "hello"), 10);
  EXPECT_FALSE(entries.reloadIfChanged());
  // The own write is not a change.
  ASSERT_TRUE(entries.add(RemovedEntries::kEnglish, "world", 20));
  EXPECT_FALSE(entries.reloadIfChanged());
}

}  // namespace McBopomofo::SmartLearning
