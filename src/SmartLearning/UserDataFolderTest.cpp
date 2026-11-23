#include "UserDataFolder.h"

#include <gtest/gtest.h>
#include <unistd.h>

#include <cstdlib>
#include <fstream>
#include <string>

namespace McBopomofo::SmartLearning {
namespace fs = std::filesystem;

class UserDataFolderTest : public ::testing::Test {
 protected:
  void SetUp() override {
    const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
    tmp_ = fs::temp_directory_path() /
           (std::string("udf-") + info->name() + "-" +
            std::to_string(::getpid()));
    fs::remove_all(tmp_);
    fs::create_directories(tmp_);
    def_ = tmp_ / "default";
    const char* home = std::getenv("HOME");
    if (home != nullptr) {
      oldHome_ = home;
      hadHome_ = true;
    }
  }
  void TearDown() override {
    if (hadHome_) {
      setenv("HOME", oldHome_.c_str(), 1);
    } else {
      unsetenv("HOME");
    }
    fs::remove_all(tmp_);
  }
  fs::path tmp_;
  fs::path def_;
  std::string w_;
  std::string oldHome_;
  bool hadHome_ = false;
};

TEST_F(UserDataFolderTest, EmptyMeansDefault) {
  EXPECT_EQ(resolveUserDataFolder("", def_, &w_), def_);
  EXPECT_TRUE(w_.empty());
  EXPECT_TRUE(fs::is_directory(def_));
}

TEST_F(UserDataFolderTest, ExpandsTilde) {
  fs::create_directories(tmp_ / "SynologyDrive/McBopomofo");
  setenv("HOME", tmp_.c_str(), 1);
  EXPECT_EQ(resolveUserDataFolder("~/SynologyDrive/McBopomofo", def_, &w_),
            tmp_ / "SynologyDrive/McBopomofo");
  EXPECT_TRUE(w_.empty());
}

TEST_F(UserDataFolderTest, ExistingDirectoryIsReturned) {
  fs::create_directories(tmp_ / "x");
  EXPECT_EQ(resolveUserDataFolder((tmp_ / "x").string(), def_, &w_),
            tmp_ / "x");
  EXPECT_TRUE(w_.empty());
}

TEST_F(UserDataFolderTest, CreatesLeafWhenParentExists) {
  fs::create_directories(tmp_ / "a");
  auto p = resolveUserDataFolder((tmp_ / "a/McBopomofo").string(), def_, &w_);
  EXPECT_EQ(p, tmp_ / "a/McBopomofo");
  EXPECT_TRUE(fs::is_directory(p));
  EXPECT_TRUE(w_.empty());
}

TEST_F(UserDataFolderTest, FallsBackWhenParentMissing) {
  auto p = resolveUserDataFolder((tmp_ / "no/such/dir").string(), def_, &w_);
  EXPECT_EQ(p, def_);
  EXPECT_FALSE(w_.empty());
  EXPECT_FALSE(fs::exists(tmp_ / "no"));
}

TEST_F(UserDataFolderTest, FallsBackWhenPathIsAFile) {
  std::ofstream(tmp_ / "file") << "x";
  auto p = resolveUserDataFolder((tmp_ / "file").string(), def_, &w_);
  EXPECT_EQ(p, def_);
  EXPECT_FALSE(w_.empty());
}

TEST_F(UserDataFolderTest, FallsBackOnRelativePath) {
  auto p = resolveUserDataFolder("some/relative", def_, &w_);
  EXPECT_EQ(p, def_);
  EXPECT_FALSE(w_.empty());
}

}  // namespace McBopomofo::SmartLearning
