#include "TemporaryPhrases.h"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "RemovedEntries.h"

namespace McBopomofo::SmartLearning {
namespace fs = std::filesystem;

namespace {

constexpr double kNow = 1700000000;

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

std::string line(const std::string& value, const std::string& reading,
                 double time) {
  return value + " " + reading + " " +
         std::to_string(static_cast<long long>(time)) + "\n";
}

class TemporaryPhrasesTest : public ::testing::Test {
 protected:
  void SetUp() override {
    auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    root_ = fs::temp_directory_path() /
            ("smartlearning-temporary-" + std::to_string(stamp) + "-" +
             ::testing::UnitTest::GetInstance()->current_test_info()->name());
    learning_ = root_ / "learning";
    temporary_ = learning_ / "temporary";
    fs::create_directories(root_);
  }
  void TearDown() override {
    std::error_code ec;
    fs::remove_all(root_, ec);
  }

  std::unique_ptr<TemporaryPhraseStore> makeStore() {
    removed_ = std::make_shared<RemovedEntries>([this] { return now_; });
    return std::make_unique<TemporaryPhraseStore>(
        "TESTDEV1", "test", [this] { return now_; }, removed_);
  }

  fs::path ownFile() const { return temporary_ / "TESTDEV1-test.txt"; }

  fs::path root_;
  fs::path learning_;
  fs::path temporary_;
  double now_ = kNow;
  std::shared_ptr<RemovedEntries> removed_;
};

}  // namespace

TEST_F(TemporaryPhrasesTest, MergesOtherDevicesAndKeepsTheNewerTime) {
  writeFile(ownFile(), line("甲乙", "ㄐㄧㄚˇ-ㄧˇ", kNow - 300));
  writeFile(temporary_ / "TESTDEV2-other.txt",
            line("甲乙", "ㄐㄧㄚˇ-ㄧˇ", kNow - 100) +
                line("丙丁", "ㄅㄧㄥˇ-ㄉㄧㄥ", kNow - 200));
  // Neither conflict copies nor files other than .txt are read.
  writeFile(temporary_ / "TESTDEV2-other.sync-conflict-1.txt",
            line("戊己", "ㄨˋ-ㄐㄧˇ", kNow));
  writeFile(temporary_ / "TESTDEV3-other.dat", line("庚辛", "ㄍㄥ-ㄒㄧㄣ", kNow));
  // Removed after the own time but before the other device's: the phrase is
  // still there, from the other device.
  writeFile(learning_ / "removed.txt",
            "temporary\t甲乙 ㄐㄧㄚˇ-ㄧˇ\t" +
                std::to_string(static_cast<long long>(kNow - 200)) + "\n");

  auto store = makeStore();
  store->setLearningFolder(learning_);
  EXPECT_TRUE(store->contains("甲乙", "ㄐㄧㄚˇ-ㄧˇ"));
  EXPECT_TRUE(store->contains("丙丁", "ㄅㄧㄥˇ-ㄉㄧㄥ"));
  EXPECT_FALSE(store->contains("戊己", "ㄨˋ-ㄐㄧˇ"));
  EXPECT_FALSE(store->contains("庚辛", "ㄍㄥ-ㄒㄧㄣ"));
  EXPECT_FALSE(store->contains("甲", "ㄐㄧㄚˇ-ㄧˇ"));

  // Saving writes only the own phrases, and the removed own one is gone.
  store->add("壬癸", "ㄖㄣˊ-ㄍㄨㄟˇ");
  ASSERT_TRUE(store->save());
  EXPECT_EQ(readFile(ownFile()), line("壬癸", "ㄖㄣˊ-ㄍㄨㄟˇ", kNow));
  EXPECT_EQ(readFile(temporary_ / "TESTDEV2-other.txt"),
            line("甲乙", "ㄐㄧㄚˇ-ㄧˇ", kNow - 100) +
                line("丙丁", "ㄅㄧㄥˇ-ㄉㄧㄥ", kNow - 200));
}

TEST_F(TemporaryPhrasesTest, PhrasesExpireAfterThirtyDays) {
  const double lifetime = TemporaryPhraseStore::kLifetime;
  EXPECT_EQ(lifetime, 30 * 86400);
  writeFile(ownFile(), line("甲乙", "ㄐㄧㄚˇ-ㄧˇ", kNow - lifetime) +
                           line("丙丁", "ㄅㄧㄥˇ-ㄉㄧㄥ", kNow - lifetime - 1) +
                           "戊己 ㄨˋ-ㄐㄧˇ soon\n");
  writeFile(temporary_ / "TESTDEV2-other.txt",
            line("庚辛", "ㄍㄥ-ㄒㄧㄣ", kNow - lifetime) +
                line("壬癸", "ㄖㄣˊ-ㄍㄨㄟˇ", kNow - lifetime - 1));
  auto store = makeStore();
  store->setLearningFolder(learning_);
  EXPECT_TRUE(store->contains("甲乙", "ㄐㄧㄚˇ-ㄧˇ"));
  EXPECT_FALSE(store->contains("丙丁", "ㄅㄧㄥˇ-ㄉㄧㄥ"));
  EXPECT_FALSE(store->contains("戊己", "ㄨˋ-ㄐㄧˇ"));
  EXPECT_TRUE(store->contains("庚辛", "ㄍㄥ-ㄒㄧㄣ"));
  EXPECT_FALSE(store->contains("壬癸", "ㄖㄣˊ-ㄍㄨㄟˇ"));

  // The other devices' phrases expire on the next reload; the own ones only
  // when the own file is read again.
  now_ += 1;
  store->reload();
  EXPECT_TRUE(store->contains("甲乙", "ㄐㄧㄚˇ-ㄧˇ"));
  EXPECT_FALSE(store->contains("庚辛", "ㄍㄥ-ㄒㄧㄣ"));
}

TEST_F(TemporaryPhrasesTest, RemovedAtOrAfterThePickTimeIsLeftOut) {
  writeFile(ownFile(), line("甲乙", "ㄐㄧㄚˇ-ㄧˇ", kNow - 100) +
                           line("丙丁", "ㄅㄧㄥˇ-ㄉㄧㄥ", kNow - 100));
  writeFile(temporary_ / "TESTDEV2-other.txt",
            line("戊己", "ㄨˋ-ㄐㄧˇ", kNow - 100) +
                line("庚辛", "ㄍㄥ-ㄒㄧㄣ", kNow - 100));
  auto at = [](double t) {
    return std::to_string(static_cast<long long>(t));
  };
  writeFile(learning_ / "removed.txt",
            "temporary\t甲乙 ㄐㄧㄚˇ-ㄧˇ\t" + at(kNow - 100) + "\n" +
                "temporary\t丙丁 ㄅㄧㄥˇ-ㄉㄧㄥ\t" + at(kNow - 101) + "\n" +
                "temporary\t戊己 ㄨˋ-ㄐㄧˇ\t" + at(kNow - 50) + "\n" +
                "temporary\t庚辛 ㄍㄥ-ㄒㄧㄣ\t" + at(kNow - 101) + "\n" +
                // Another kind with the same key does not count.
                "sentence\t丙丁 ㄅㄧㄥˇ-ㄉㄧㄥ\t" + at(kNow) + "\n");
  auto store = makeStore();
  store->setLearningFolder(learning_);
  EXPECT_FALSE(store->contains("甲乙", "ㄐㄧㄚˇ-ㄧˇ"));
  EXPECT_TRUE(store->contains("丙丁", "ㄅㄧㄥˇ-ㄉㄧㄥ"));
  EXPECT_FALSE(store->contains("戊己", "ㄨˋ-ㄐㄧˇ"));
  EXPECT_TRUE(store->contains("庚辛", "ㄍㄥ-ㄒㄧㄣ"));

  // Picked again after it was removed, it is back.
  store->add("甲乙", "ㄐㄧㄚˇ-ㄧˇ");
  EXPECT_TRUE(store->contains("甲乙", "ㄐㄧㄚˇ-ㄧˇ"));
  store->reload();
  EXPECT_TRUE(store->contains("甲乙", "ㄐㄧㄚˇ-ㄧˇ"));
}

TEST_F(TemporaryPhrasesTest, NeverWritesBeforeTheOwnFileLoads) {
  // The user data folder itself is missing, for example a sync folder that
  // has not arrived yet.
  const fs::path dataFolder = root_ / "data";
  const fs::path learning = dataFolder / "learning";
  const fs::path own = learning / "temporary" / "TESTDEV1-test.txt";
  auto store = makeStore();
  store->setLearningFolder(learning);
  EXPECT_TRUE(store->isLocalFilePending());
  store->add("甲乙", "ㄐㄧㄚˇ-ㄧˇ");
  EXPECT_TRUE(store->contains("甲乙", "ㄐㄧㄚˇ-ㄧˇ"));
  EXPECT_TRUE(store->hasUnsavedChanges());
  EXPECT_FALSE(store->save());
  EXPECT_FALSE(fs::exists(dataFolder));
  EXPECT_TRUE(store->hasUnsavedChanges());

  // The own file arrives: its phrases are kept, with what was added meanwhile.
  writeFile(own, line("丙丁", "ㄅㄧㄥˇ-ㄉㄧㄥ", kNow - 10) +
                     line("甲乙", "ㄐㄧㄚˇ-ㄧˇ", kNow - 20));
  ASSERT_TRUE(store->save());
  EXPECT_FALSE(store->isLocalFilePending());
  EXPECT_FALSE(store->hasUnsavedChanges());
  EXPECT_EQ(readFile(own), line("丙丁", "ㄅㄧㄥˇ-ㄉㄧㄥ", kNow - 10) +
                               line("甲乙", "ㄐㄧㄚˇ-ㄧˇ", kNow));
}

TEST_F(TemporaryPhrasesTest, RemovedWhilePendingStaysRemoved) {
  const fs::path learning = root_ / "data" / "learning";
  const fs::path own = learning / "temporary" / "TESTDEV1-test.txt";
  auto store = makeStore();
  store->setLearningFolder(learning);
  store->add("甲乙", "ㄐㄧㄚˇ-ㄧˇ");
  store->remove("甲乙", "ㄐㄧㄚˇ-ㄧˇ");
  EXPECT_FALSE(store->contains("甲乙", "ㄐㄧㄚˇ-ㄧˇ"));

  writeFile(own, line("甲乙", "ㄐㄧㄚˇ-ㄧˇ", kNow - 20) +
                     line("丙丁", "ㄅㄧㄥˇ-ㄉㄧㄥ", kNow - 10));
  store->reload();
  EXPECT_FALSE(store->isLocalFilePending());
  EXPECT_FALSE(store->contains("甲乙", "ㄐㄧㄚˇ-ㄧˇ"));
  EXPECT_TRUE(store->contains("丙丁", "ㄅㄧㄥˇ-ㄉㄧㄥ"));
  ASSERT_TRUE(store->save());
  EXPECT_EQ(readFile(own), line("丙丁", "ㄅㄧㄥˇ-ㄉㄧㄥ", kNow - 10));
}

TEST_F(TemporaryPhrasesTest, MissingTemporaryFolderLoadsEmptyAndSaveCreatesIt) {
  // root_ is the user data folder; neither learning/ nor temporary/ exists.
  auto store = makeStore();
  store->setLearningFolder(learning_);
  EXPECT_FALSE(store->isLocalFilePending());
  EXPECT_FALSE(fs::exists(learning_));
  store->add("甲乙", "ㄐㄧㄚˇ-ㄧˇ");
  ASSERT_TRUE(store->save());
  EXPECT_EQ(readFile(ownFile()), line("甲乙", "ㄐㄧㄚˇ-ㄧˇ", kNow));

  fs::remove_all(temporary_);
  auto other = makeStore();
  other->setLearningFolder(learning_);
  EXPECT_FALSE(other->isLocalFilePending());
  other->add("丙丁", "ㄅㄧㄥˇ-ㄉㄧㄥ");
  ASSERT_TRUE(other->save());
  EXPECT_EQ(readFile(ownFile()), line("丙丁", "ㄅㄧㄥˇ-ㄉㄧㄥ", kNow));
}

TEST_F(TemporaryPhrasesTest, BlockedOrUnreadableOwnFileStaysPending) {
  writeFile(temporary_, "synthetic");  // a file where the folder should be
  auto store = makeStore();
  store->setLearningFolder(learning_);
  store->add("甲乙", "ㄐㄧㄚˇ-ㄧˇ");
  EXPECT_FALSE(store->save());
  EXPECT_EQ(readFile(temporary_), "synthetic");

  fs::remove(temporary_);
  fs::create_directories(ownFile());  // a folder where the file should be
  auto other = makeStore();
  other->setLearningFolder(learning_);
  other->add("甲乙", "ㄐㄧㄚˇ-ㄧˇ");
  EXPECT_TRUE(other->isLocalFilePending());
  EXPECT_FALSE(other->save());
  EXPECT_TRUE(fs::is_directory(ownFile()));
}

TEST_F(TemporaryPhrasesTest, AddMarksUnsavedAndSavesSortedWholeSeconds) {
  fs::create_directories(temporary_);
  auto store = makeStore();
  store->setLearningFolder(learning_);
  EXPECT_FALSE(store->hasUnsavedChanges());
  now_ = kNow + 0.6;
  store->add("丙丁", "ㄅㄧㄥˇ-ㄉㄧㄥ");
  now_ = kNow + 1.2;
  store->add("甲乙", "ㄐㄧㄚˇ-ㄧˇ");
  EXPECT_TRUE(store->hasUnsavedChanges());
  // Adding does not write by itself.
  EXPECT_FALSE(fs::exists(ownFile()));
  ASSERT_TRUE(store->save());
  EXPECT_FALSE(store->hasUnsavedChanges());
  EXPECT_EQ(readFile(ownFile()), line("丙丁", "ㄅㄧㄥˇ-ㄉㄧㄥ", kNow + 1) +
                                     line("甲乙", "ㄐㄧㄚˇ-ㄧˇ", kNow + 1));
}

TEST_F(TemporaryPhrasesTest, AddIgnoresWhatTheFileCannotHold) {
  fs::create_directories(temporary_);
  auto store = makeStore();
  store->setLearningFolder(learning_);
  store->add("甲 乙", "ㄐㄧㄚˇ-ㄧˇ");
  store->add("甲乙", "ㄐㄧㄚˇ ㄧˇ");
  store->add("甲\n乙", "ㄐㄧㄚˇ-ㄧˇ");
  store->add("", "ㄐㄧㄚˇ-ㄧˇ");
  store->add("甲乙", "");
  EXPECT_FALSE(store->hasUnsavedChanges());
  EXPECT_TRUE(store->phrasesByReading().empty());
}

TEST_F(TemporaryPhrasesTest, RemoveTakesThePhraseOutWithoutRemovedFile) {
  writeFile(ownFile(), line("甲乙", "ㄐㄧㄚˇ-ㄧˇ", kNow - 10));
  writeFile(temporary_ / "TESTDEV2-other.txt",
            line("丙丁", "ㄅㄧㄥˇ-ㄉㄧㄥ", kNow - 10));
  auto store = makeStore();
  store->setLearningFolder(learning_);

  store->remove("戊己", "ㄨˋ-ㄐㄧˇ");
  EXPECT_FALSE(store->hasUnsavedChanges());

  store->remove("甲乙", "ㄐㄧㄚˇ-ㄧˇ");
  EXPECT_FALSE(store->contains("甲乙", "ㄐㄧㄚˇ-ㄧˇ"));
  EXPECT_TRUE(store->hasUnsavedChanges());
  ASSERT_TRUE(store->save());
  EXPECT_EQ(readFile(ownFile()), "");
  EXPECT_FALSE(fs::exists(learning_ / "removed.txt"));

  // Another device's phrase is taken out until the next reload, leaving
  // the own file as it is.
  store->remove("丙丁", "ㄅㄧㄥˇ-ㄉㄧㄥ");
  EXPECT_FALSE(store->contains("丙丁", "ㄅㄧㄥˇ-ㄉㄧㄥ"));
  EXPECT_FALSE(store->hasUnsavedChanges());
  store->reload();
  EXPECT_TRUE(store->contains("丙丁", "ㄅㄧㄥˇ-ㄉㄧㄥ"));
}

TEST_F(TemporaryPhrasesTest, PhrasesByReadingGroupsNewestFirst) {
  writeFile(ownFile(), line("丁戊", "ㄉㄧㄥ-ㄨˋ", kNow - 30) +
                           line("頂物", "ㄉㄧㄥ-ㄨˋ", kNow - 10) +
                           line("甲乙", "ㄐㄧㄚˇ-ㄧˇ", kNow - 20));
  writeFile(temporary_ / "TESTDEV2-other.txt",
            line("丁午", "ㄉㄧㄥ-ㄨˋ", kNow - 20) +
                // Lines without exactly three columns are skipped.
                "丁武 ㄉㄧㄥ-ㄨˋ\n" + "丁 舞 ㄉㄧㄥ-ㄨˋ " +
                std::to_string(static_cast<long long>(kNow)) + "\n");
  auto store = makeStore();
  store->setLearningFolder(learning_);
  auto phrases = store->phrasesByReading();
  std::unordered_map<std::string, std::vector<std::string>> expected = {
      {"ㄉㄧㄥ-ㄨˋ", {"頂物", "丁午", "丁戊"}},
      {"ㄐㄧㄚˇ-ㄧˇ", {"甲乙"}},
  };
  EXPECT_EQ(phrases, expected);
}

TEST_F(TemporaryPhrasesTest, OwnFileChangedElsewhereIsMergedNotReplaced) {
  writeFile(ownFile(), line("甲乙", "ㄐㄧㄚˇ-ㄧˇ", kNow - 10));
  auto store = makeStore();
  store->setLearningFolder(learning_);
  // A phrase missing from the file is not taken as removed: only removed.txt
  // removes.
  writeFile(ownFile(), line("丙丁", "ㄅㄧㄥˇ-ㄉㄧㄥ", kNow - 10));
  store->reload();
  EXPECT_TRUE(store->contains("甲乙", "ㄐㄧㄚˇ-ㄧˇ"));
  EXPECT_TRUE(store->contains("丙丁", "ㄅㄧㄥˇ-ㄉㄧㄥ"));
}

TEST_F(TemporaryPhrasesTest, ReloadIfChangedSeesOtherDevices) {
  fs::create_directories(temporary_);
  auto store = makeStore();
  store->setLearningFolder(learning_);
  EXPECT_FALSE(store->reloadIfChanged());
  writeFile(temporary_ / "TESTDEV2-other.txt",
            line("丙丁", "ㄅㄧㄥˇ-ㄉㄧㄥ", kNow - 10));
  EXPECT_TRUE(store->reloadIfChanged());
  EXPECT_TRUE(store->contains("丙丁", "ㄅㄧㄥˇ-ㄉㄧㄥ"));
  EXPECT_FALSE(store->reloadIfChanged());

  // The own save is not taken for a change.
  store->add("甲乙", "ㄐㄧㄚˇ-ㄧˇ");
  ASSERT_TRUE(store->save());
  EXPECT_FALSE(store->reloadIfChanged());
}

TEST_F(TemporaryPhrasesTest, ReloadIfChangedSeesANewRemovalAtOnce) {
  writeFile(temporary_ / "TESTDEV2-other.txt",
            line("丙丁", "ㄅㄧㄥˇ-ㄉㄧㄥ", kNow - 10));
  auto store = makeStore();
  store->setLearningFolder(learning_);
  EXPECT_TRUE(store->contains("丙丁", "ㄅㄧㄥˇ-ㄉㄧㄥ"));
  // Well within the reload interval of the removed entries.
  writeFile(learning_ / "removed.txt",
            "temporary\t丙丁 ㄅㄧㄥˇ-ㄉㄧㄥ\t" +
                std::to_string(static_cast<long long>(kNow)) + "\n");
  EXPECT_TRUE(store->reloadIfChanged());
  EXPECT_FALSE(store->contains("丙丁", "ㄅㄧㄥˇ-ㄉㄧㄥ"));
}

TEST_F(TemporaryPhrasesTest, ReloadIfChangedRetriesAPendingOwnFile) {
  const fs::path learning = root_ / "data" / "learning";
  auto store = makeStore();
  store->setLearningFolder(learning);
  EXPECT_FALSE(store->reloadIfChanged());
  writeFile(learning / "temporary" / "TESTDEV1-test.txt",
            line("甲乙", "ㄐㄧㄚˇ-ㄧˇ", kNow - 10));
  EXPECT_TRUE(store->reloadIfChanged());
  EXPECT_FALSE(store->isLocalFilePending());
  EXPECT_TRUE(store->contains("甲乙", "ㄐㄧㄚˇ-ㄧˇ"));
}

TEST_F(TemporaryPhrasesTest, SwitchingFoldersSavesOldThenStartsOver) {
  fs::create_directories(temporary_);
  const fs::path second = root_ / "second" / "learning";
  writeFile(second / "temporary" / "TESTDEV1-test.txt",
            line("丙丁", "ㄅㄧㄥˇ-ㄉㄧㄥ", kNow - 10));
  auto store = makeStore();
  store->setLearningFolder(learning_);
  store->add("甲乙", "ㄐㄧㄚˇ-ㄧˇ");
  store->setLearningFolder(second);
  EXPECT_EQ(readFile(ownFile()), line("甲乙", "ㄐㄧㄚˇ-ㄧˇ", kNow));
  EXPECT_FALSE(store->contains("甲乙", "ㄐㄧㄚˇ-ㄧˇ"));
  EXPECT_TRUE(store->contains("丙丁", "ㄅㄧㄥˇ-ㄉㄧㄥ"));
  EXPECT_FALSE(store->hasUnsavedChanges());

  // Empty and relative folders are ignored.
  store->setLearningFolder("");
  store->setLearningFolder("relative/learning");
  EXPECT_TRUE(store->contains("丙丁", "ㄅㄧㄥˇ-ㄉㄧㄥ"));
}

TEST_F(TemporaryPhrasesTest, NoFolderMeansNothingIsWritten) {
  auto store = makeStore();
  store->add("甲乙", "ㄐㄧㄚˇ-ㄧˇ");
  EXPECT_TRUE(store->contains("甲乙", "ㄐㄧㄚˇ-ㄧˇ"));
  EXPECT_FALSE(store->save());
}

// MARK: - Vocabulary manager

std::string removedLine(const std::string& key, double time) {
  return "temporary\t" + key + "\t" +
         std::to_string(static_cast<long long>(time)) + "\n";
}

TEST_F(TemporaryPhrasesTest, AllPhrasesNewestFirstWithTheirTimes) {
  writeFile(ownFile(), line("甲乙", "ㄐㄧㄚˇ-ㄧˇ", kNow - 30) +
                           line("丙丁", "ㄅㄧㄥˇ-ㄉㄧㄥ", kNow - 10));
  auto store = makeStore();
  store->setLearningFolder(learning_);
  // Written elsewhere since: listing reads it again.
  writeFile(temporary_ / "TESTDEV2-other.txt",
            line("戊己", "ㄨˋ-ㄐㄧˇ", kNow - 20) +
                line("甲乙", "ㄐㄧㄚˇ-ㄧˇ", kNow - 5));
  auto phrases = store->allPhrases();
  ASSERT_EQ(phrases.size(), 3U);
  EXPECT_EQ(phrases[0].value, "甲乙");
  EXPECT_EQ(phrases[0].reading, "ㄐㄧㄚˇ-ㄧˇ");
  EXPECT_EQ(phrases[0].time, kNow - 5);
  EXPECT_EQ(phrases[1].value, "丙丁");
  EXPECT_EQ(phrases[2].value, "戊己");
}

TEST_F(TemporaryPhrasesTest, ForgetRecordsTheRemovalAndSaves) {
  writeFile(ownFile(), line("甲乙", "ㄐㄧㄚˇ-ㄧˇ", kNow - 30) +
                           line("丙丁", "ㄅㄧㄥˇ-ㄉㄧㄥ", kNow - 10));
  writeFile(temporary_ / "TESTDEV2-other.txt",
            line("甲乙", "ㄐㄧㄚˇ-ㄧˇ", kNow - 20));
  auto store = makeStore();
  store->setLearningFolder(learning_);
  now_ += 5;
  ASSERT_TRUE(store->forget("甲乙", "ㄐㄧㄚˇ-ㄧˇ"));
  EXPECT_FALSE(store->contains("甲乙", "ㄐㄧㄚˇ-ㄧˇ"));
  EXPECT_FALSE(store->hasUnsavedChanges());
  EXPECT_EQ(readFile(ownFile()), line("丙丁", "ㄅㄧㄥˇ-ㄉㄧㄥ", kNow - 10));
  EXPECT_EQ(readFile(learning_ / "removed.txt"),
            removedLine("甲乙 ㄐㄧㄚˇ-ㄧˇ", now_));
  // The other device's file is left as it is, and stays hidden.
  EXPECT_EQ(readFile(temporary_ / "TESTDEV2-other.txt"),
            line("甲乙", "ㄐㄧㄚˇ-ㄧˇ", kNow - 20));
  store->reload();
  EXPECT_FALSE(store->contains("甲乙", "ㄐㄧㄚˇ-ㄧˇ"));
}

TEST_F(TemporaryPhrasesTest, ForgetWritesNothingWhilePending) {
  const fs::path learning = root_ / "data" / "learning";
  auto store = makeStore();
  store->setLearningFolder(learning);
  ASSERT_TRUE(store->isLocalFilePending());
  EXPECT_FALSE(store->forget("甲乙", "ㄐㄧㄚˇ-ㄧˇ"));
  EXPECT_FALSE(fs::exists(root_ / "data"));
  EXPECT_TRUE(store->allPhrases().empty());
}

TEST_F(TemporaryPhrasesTest, APhraseForgottenByAnotherProgramStaysForgotten) {
  writeFile(ownFile(), line("甲乙", "ㄐㄧㄚˇ-ㄧˇ", kNow - 30) +
                           line("丙丁", "ㄅㄧㄥˇ-ㄉㄧㄥ", kNow - 10));
  auto ime = makeStore();
  ime->setLearningFolder(learning_);
  auto manager = makeStore();
  manager->setLearningFolder(learning_);
  now_ += 1;
  ASSERT_TRUE(manager->forget("甲乙", "ㄐㄧㄚˇ-ㄧˇ"));

  // The input method saves a phrase picked since, without having reloaded.
  ime->add("戊己", "ㄨˋ-ㄐㄧˇ");
  ASSERT_TRUE(ime->save());
  EXPECT_EQ(readFile(ownFile()), line("丙丁", "ㄅㄧㄥˇ-ㄉㄧㄥ", kNow - 10) +
                                     line("戊己", "ㄨˋ-ㄐㄧˇ", now_));
  EXPECT_FALSE(ime->contains("甲乙", "ㄐㄧㄚˇ-ㄧˇ"));

  // The manager reads the own file again when it changed, the later time of
  // each phrase winning.
  writeFile(ownFile(), line("丙丁", "ㄅㄧㄥˇ-ㄉㄧㄥ", kNow + 50) +
                           line("戊己", "ㄨˋ-ㄐㄧˇ", kNow - 100) +
                           line("庚辛", "ㄍㄥ-ㄒㄧㄣ", kNow - 200));
  auto phrases = manager->allPhrases();
  ASSERT_EQ(phrases.size(), 3U);
  EXPECT_EQ(phrases[0].value, "丙丁");
  EXPECT_EQ(phrases[0].time, kNow + 50);
  EXPECT_EQ(phrases[1].value, "戊己");
  EXPECT_EQ(phrases[2].value, "庚辛");
}

}  // namespace McBopomofo::SmartLearning
