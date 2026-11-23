#include "LearningPaths.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <fstream>

namespace McBopomofo::SmartLearning {
namespace fs = std::filesystem;

class LearningPathsFolderTest : public ::testing::Test {
 protected:
  void SetUp() override {
    auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    folder_ = fs::temp_directory_path() /
              ("smartlearning-paths-" + std::to_string(stamp) + "-" +
               ::testing::UnitTest::GetInstance()->current_test_info()->name());
    fs::create_directories(folder_);
  }
  void TearDown() override {
    std::error_code ec;
    fs::remove_all(folder_, ec);
  }
  void touch(const std::string& name) {
    std::ofstream(folder_ / fs::u8path(name)) << "x";
  }
  fs::path folder_;
};

TEST(LearningPathsTest, DeviceIDIsEightUppercaseHex) {
  auto id = newDeviceID();
  EXPECT_EQ(id.size(), 8u);
  EXPECT_TRUE(std::all_of(id.begin(), id.end(), [](char c) {
    return std::isxdigit(static_cast<unsigned char>(c)) &&
           !std::islower(static_cast<unsigned char>(c));
  }));
}

TEST(LearningPathsTest, SanitizesLikeMac) {
  EXPECT_EQ(learningFilename("1778D9F7", sanitizeDeviceName("Mac Studio")),
            "1778D9F7-Mac Studio.txt");
  EXPECT_EQ(sanitizeDeviceName("a/b:c\\d\te"), "a_b_c_d_e");
  EXPECT_EQ(sanitizeDeviceName("a\nb\rc"), "a_b_c");
  EXPECT_EQ(sanitizeDeviceName("筆電"), "筆電");
  EXPECT_EQ(sanitizeDeviceName(""), "Linux");
}

TEST(LearningPathsTest, DetectsConflictCopies) {
  EXPECT_TRUE(isSyncConflictCopy("A-b_x_Conflict.txt"));
  EXPECT_TRUE(isSyncConflictCopy("A-b.sync-conflict-1-2-X.txt"));
  EXPECT_FALSE(isSyncConflictCopy("A-b.txt"));
  EXPECT_FALSE(isSyncConflictCopy("A-b_conflict.txt"));
}

TEST_F(LearningPathsFolderTest, SkipsConflictCopiesAndOwnFile) {
  touch("AAAA0001-mint.txt");
  touch("1778D9F7-Mac Studio.txt");
  touch("523308DB-MacBook Neo_mint_Nov-23-120000-2026_Conflict.txt");
  touch("1778D9F7-Mac Studio.sync-conflict-20261123-120000-ABC.txt");
  touch(".hidden.txt");
  touch("notes.md");
  fs::create_directories(folder_ / "temporary");
  touch("temporary/BBBB0002-x.txt");

  auto files = otherDevicesLearningFiles(folder_, "AAAA0001-mint.txt");
  ASSERT_EQ(files.size(), 1u);
  EXPECT_EQ(files[0].filename(), "1778D9F7-Mac Studio.txt");
}

TEST_F(LearningPathsFolderTest, OtherDevicesFilesAreSorted) {
  touch("CCCC0003-c.txt");
  touch("AAAA0001-a.txt");
  touch("BBBB0002-b.txt");
  auto files = otherDevicesLearningFiles(folder_, "ZZZZ0000-z.txt");
  ASSERT_EQ(files.size(), 3u);
  EXPECT_EQ(files[0].filename(), "AAAA0001-a.txt");
  EXPECT_EQ(files[1].filename(), "BBBB0002-b.txt");
  EXPECT_EQ(files[2].filename(), "CCCC0003-c.txt");
}

TEST_F(LearningPathsFolderTest, MissingFolderGivesEmptyResult) {
  auto missing = folder_ / "nope";
  EXPECT_TRUE(otherDevicesLearningFiles(missing, "A-b.txt").empty());
  EXPECT_FALSE(findRenamedOwnFile(missing, "AAAA0001", "AAAA0001-n.txt"));
}

TEST_F(LearningPathsFolderTest, FindsOwnFileAfterHostnameChange) {
  touch("AAAA0001-oldname.txt");
  touch("AAAA0001-oldname_x_Conflict.txt");
  touch("BBBB0002-other.txt");
  auto found = findRenamedOwnFile(folder_, "AAAA0001", "AAAA0001-newname.txt");
  ASSERT_TRUE(found.has_value());
  EXPECT_EQ(found->filename(), "AAAA0001-oldname.txt");
}

TEST_F(LearningPathsFolderTest, RenamedOwnFileIgnoresCurrentAndConflicts) {
  touch("AAAA0001-newname.txt");
  touch("AAAA0001-x.sync-conflict-1-2-Q.txt");
  EXPECT_FALSE(
      findRenamedOwnFile(folder_, "AAAA0001", "AAAA0001-newname.txt"));
}

TEST_F(LearningPathsFolderTest, RenamedOwnFileTakesFirstInSortedOrder) {
  touch("AAAA0001-b.txt");
  touch("AAAA0001-a.txt");
  auto found = findRenamedOwnFile(folder_, "AAAA0001", "AAAA0001-new.txt");
  ASSERT_TRUE(found.has_value());
  EXPECT_EQ(found->filename(), "AAAA0001-a.txt");
}

TEST_F(LearningPathsFolderTest, LearningFolderState) {
  const fs::path learning = folder_ / "learning";
  const fs::path english = learning / "english";
  EXPECT_EQ(learningFolderState(folder_, 1), LearningFolderState::kExists);
  EXPECT_EQ(learningFolderState(learning, 1), LearningFolderState::kCreatable);
  EXPECT_EQ(learningFolderState(english, 1),
            LearningFolderState::kUnavailable);
  EXPECT_EQ(learningFolderState(english, 2), LearningFolderState::kCreatable);
  EXPECT_EQ(learningFolderState(folder_ / "data" / "learning", 1),
            LearningFolderState::kUnavailable);
  EXPECT_EQ(learningFolderState(folder_ / "data" / "learning" / "english", 2),
            LearningFolderState::kUnavailable);

  fs::create_directories(learning);
  EXPECT_EQ(learningFolderState(learning / "", 1),
            LearningFolderState::kExists);
  EXPECT_EQ(learningFolderState(english, 1), LearningFolderState::kCreatable);

  // A file in the way of the folder or of a missing parent.
  touch("learning/english");
  EXPECT_EQ(learningFolderState(english, 2),
            LearningFolderState::kUnavailable);
  EXPECT_EQ(learningFolderState(english / "more", 2),
            LearningFolderState::kUnavailable);
}

}  // namespace McBopomofo::SmartLearning
