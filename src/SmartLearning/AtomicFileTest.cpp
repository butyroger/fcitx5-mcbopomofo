#include "AtomicFile.h"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace McBopomofo::SmartLearning {
namespace fs = std::filesystem;

class AtomicFileTest : public ::testing::Test {
 protected:
  void SetUp() override {
    auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    folder_ = fs::temp_directory_path() /
              ("smartlearning-atomic-" + std::to_string(stamp) + "-" +
               ::testing::UnitTest::GetInstance()->current_test_info()->name());
    fs::create_directories(folder_);
  }
  void TearDown() override {
    std::error_code ec;
    fs::remove_all(folder_, ec);
  }
  static std::string slurp(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
  }
  size_t entryCount() const {
    size_t n = 0;
    for (const auto& e : fs::directory_iterator(folder_)) {
      (void)e;
      ++n;
    }
    return n;
  }
  fs::path folder_;
};

TEST_F(AtomicFileTest, WritesContent) {
  auto path = folder_ / "a.txt";
  ASSERT_TRUE(writeFileAtomically(path, "hello\n筆電\n"));
  EXPECT_EQ(slurp(path), "hello\n筆電\n");
}

TEST_F(AtomicFileTest, OverwriteLeavesNoTempFile) {
  auto path = folder_ / "a.txt";
  ASSERT_TRUE(writeFileAtomically(path, "one"));
  ASSERT_TRUE(writeFileAtomically(path, "two"));
  EXPECT_EQ(slurp(path), "two");
  for (const auto& e : fs::directory_iterator(folder_)) {
    EXPECT_EQ(e.path().filename().string().find(".tmp-"), std::string::npos);
  }
  EXPECT_EQ(entryCount(), 1u);
}

TEST_F(AtomicFileTest, MissingDirectoryReturnsFalse) {
  EXPECT_FALSE(writeFileAtomically(folder_ / "nope" / "a.txt", "x"));
}

TEST_F(AtomicFileTest, FailedRenameCleansTempFile) {
  // Destination is a non-empty directory, so rename fails.
  auto path = folder_ / "dir";
  fs::create_directories(path / "inner");
  EXPECT_FALSE(writeFileAtomically(path, "x"));
  EXPECT_EQ(entryCount(), 1u);
}

TEST_F(AtomicFileTest, CreateFileIfAbsentCreatesNewFile) {
  auto p = folder_ / "data.txt";
  EXPECT_TRUE(createFileIfAbsent(p, "# header\n"));
  EXPECT_EQ(slurp(p), "# header\n");
}

TEST_F(AtomicFileTest, CreateFileIfAbsentNeverTouchesExistingFile) {
  auto p = folder_ / "data.txt";
  {
    std::ofstream out(p, std::ios::binary);
    out << "synthetic 1 2\nsynthetic 3 4\n";
  }
  EXPECT_FALSE(createFileIfAbsent(p, "# header\n"));
  EXPECT_EQ(slurp(p), "synthetic 1 2\nsynthetic 3 4\n");
}

TEST_F(AtomicFileTest, CreateFileIfAbsentFailsWhenFolderMissing) {
  EXPECT_FALSE(createFileIfAbsent(folder_ / "nope" / "data.txt", "x"));
}

}  // namespace McBopomofo::SmartLearning
