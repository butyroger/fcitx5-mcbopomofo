#include "AtomicFile.h"

#include <gtest/gtest.h>
#include <sys/resource.h>
#include <sys/stat.h>

#include <chrono>
#include <csignal>
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

// Limits the size of files this process may write, so that writes fail with
// EFBIG (this also applies to root). Restores the limit on destruction.
class FileSizeLimit {
 public:
  explicit FileSizeLimit(rlim_t bytes) {
    getrlimit(RLIMIT_FSIZE, &old_);
    oldHandler_ = std::signal(SIGXFSZ, SIG_IGN);
    rlimit limit = old_;
    limit.rlim_cur = bytes;
    ok_ = setrlimit(RLIMIT_FSIZE, &limit) == 0;
  }
  ~FileSizeLimit() {
    setrlimit(RLIMIT_FSIZE, &old_);
    std::signal(SIGXFSZ, oldHandler_);
  }
  bool ok() const { return ok_; }

 private:
  rlimit old_{};
  void (*oldHandler_)(int) = nullptr;
  bool ok_ = false;
};

TEST_F(AtomicFileTest, FailedWriteKeepsOldFileAndCleansTempFile) {
  auto path = folder_ / "a.txt";
  ASSERT_TRUE(writeFileAtomically(path, "old"));
  bool written = true;
  {
    FileSizeLimit limit(16);
    ASSERT_TRUE(limit.ok());
    written = writeFileAtomically(path, std::string(64 * 1024, 'x'));
  }
  EXPECT_FALSE(written);
  EXPECT_EQ(slurp(path), "old");
  EXPECT_EQ(entryCount(), 1u);
}

TEST_F(AtomicFileTest, CreateFileIfAbsentFailsOnShortWrite) {
  auto p = folder_ / "data.txt";
  bool created = true;
  {
    FileSizeLimit limit(16);
    ASSERT_TRUE(limit.ok());
    created = createFileIfAbsent(p, std::string(64 * 1024, 'x'));
  }
  EXPECT_FALSE(created);
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

// Sets the umask for the life of the object.
class ScopedUmask {
 public:
  explicit ScopedUmask(mode_t mask) : previous_(::umask(mask)) {}
  ~ScopedUmask() { ::umask(previous_); }

 private:
  mode_t previous_;
};

static mode_t permissionsOf(const fs::path& p) {
  struct stat st {};
  if (::stat(p.c_str(), &st) != 0) {
    return static_cast<mode_t>(-1);
  }
  return st.st_mode & 07777;
}

TEST_F(AtomicFileTest, KeepsPermissions0644OfExistingFile) {
  auto path = folder_ / "data.txt";
  ASSERT_TRUE(createFileIfAbsent(path, "one"));
  ASSERT_EQ(::chmod(path.c_str(), 0644), 0);
  // A strict umask must not make the shared file private.
  ScopedUmask mask(077);
  ASSERT_TRUE(writeFileAtomically(path, "two"));
  EXPECT_EQ(slurp(path), "two");
  EXPECT_EQ(permissionsOf(path), static_cast<mode_t>(0644));
}

TEST_F(AtomicFileTest, KeepsPermissions0640OfExistingFile) {
  auto path = folder_ / "data.txt";
  ASSERT_TRUE(createFileIfAbsent(path, "one"));
  ASSERT_EQ(::chmod(path.c_str(), 0640), 0);
  ScopedUmask mask(022);
  ASSERT_TRUE(writeFileAtomically(path, "two"));
  EXPECT_EQ(permissionsOf(path), static_cast<mode_t>(0640));
}

TEST_F(AtomicFileTest, NewFileGetsPermissionsFromUmask) {
  {
    ScopedUmask mask(022);
    ASSERT_TRUE(writeFileAtomically(folder_ / "a.txt", "x"));
  }
  EXPECT_EQ(permissionsOf(folder_ / "a.txt"), static_cast<mode_t>(0644));
  {
    ScopedUmask mask(002);
    ASSERT_TRUE(writeFileAtomically(folder_ / "b.txt", "x"));
  }
  EXPECT_EQ(permissionsOf(folder_ / "b.txt"), static_cast<mode_t>(0664));
}

}  // namespace McBopomofo::SmartLearning
