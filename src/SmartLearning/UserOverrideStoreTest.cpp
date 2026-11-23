#include "UserOverrideStore.h"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "UserOverrideModel.h"
#include "gramambular2/reading_grid.h"

namespace McBopomofo::SmartLearning {
namespace fs = std::filesystem;
using Formosa::Gramambular2::LanguageModel;
using Formosa::Gramambular2::ReadingGrid;

namespace {

constexpr double kNow = 1657772432;

constexpr char kSampleData[] = R"(
ㄐㄧ 機 -3.02367199
ㄐㄧ 積 -3.72854036
ㄐㄧ-ㄧㄡˊ 機油 -6.03662914
ㄧㄡˊ 由 -3.00970678
ㄧㄡˊ 油 -3.75900671
)";

class SimpleLM : public LanguageModel {
 public:
  explicit SimpleLM(const char* input) {
    std::stringstream sstream(input);
    while (sstream.good()) {
      std::string line;
      getline(sstream, line);
      if (line.empty() || line[0] == '#') {
        continue;
      }
      std::stringstream linestream(line);
      std::string reading;
      std::string value;
      std::string score;
      linestream >> reading >> value >> score;
      db_[reading].emplace_back(value, std::stod(score));
    }
  }

  std::vector<Unigram> getUnigrams(const std::string& key) override {
    const auto f = db_.find(key);
    return f == db_.end() ? std::vector<Unigram>() : f->second;
  }

  bool hasUnigrams(const std::string& key) override {
    return db_.find(key) != db_.end();
  }

 private:
  std::map<std::string, std::vector<Unigram>> db_;
};

// Two distinct observations built from real walks:
// - "oil": ㄐㄧ ㄧㄡˊ walks to 機 由, the user picks 機油 at cursor 1.
// - "accumulate": ㄐㄧ walks to 機, the user picks 積 at cursor 0.
struct Walks {
  ReadingGrid::WalkResult oilBefore;
  ReadingGrid::WalkResult oilAfter;
  ReadingGrid::WalkResult oilFresh;  // what a new walk gives before learning
  ReadingGrid::WalkResult accBefore;
  ReadingGrid::WalkResult accAfter;
  ReadingGrid::WalkResult accFresh;
};

Walks makeWalks() {
  Walks w;
  ReadingGrid grid(std::make_shared<SimpleLM>(kSampleData));
  grid.insertReading("ㄐㄧ");
  grid.insertReading("ㄧㄡˊ");
  w.oilBefore = grid.walk();
  w.oilFresh = w.oilBefore;
  grid.overrideCandidate(1, "機油");
  w.oilAfter = grid.walk();

  grid.clear();
  grid.insertReading("ㄐㄧ");
  w.accBefore = grid.walk();
  w.accFresh = w.accBefore;
  grid.overrideCandidate(0, "積");
  w.accAfter = grid.walk();
  return w;
}

// Serialized text of a model that observed only the "accumulate" choice, as
// another device would have saved it.
std::string accumulateLine(const Walks& w) {
  UserOverrideModel model(UserOverrideStore::kLocalCapacity,
                          UserOverrideStore::kHalfLife);
  model.observe(w.accBefore, w.accAfter, 0, kNow);
  return model.serialize();
}

std::string readFile(const fs::path& p) {
  std::ifstream in(p, std::ios::binary);
  std::stringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

void writeFile(const fs::path& p, const std::string& content) {
  std::ofstream out(p, std::ios::binary | std::ios::trunc);
  out << content;
}

}  // namespace

class UserOverrideStoreTest : public ::testing::Test {
 protected:
  void SetUp() override {
    auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    folder_ = fs::temp_directory_path() /
              ("smartlearning-store-" + std::to_string(stamp) + "-" +
               ::testing::UnitTest::GetInstance()->current_test_info()->name());
    fs::create_directories(folder_);
    walks_ = makeWalks();
    ASSERT_EQ(walks_.oilBefore.valuesAsStrings(),
              (std::vector<std::string>{"機", "由"}));
    ASSERT_EQ(walks_.oilAfter.valuesAsStrings(),
              (std::vector<std::string>{"機油"}));
    ASSERT_EQ(walks_.accAfter.valuesAsStrings(),
              (std::vector<std::string>{"積"}));
  }
  void TearDown() override {
    std::error_code ec;
    fs::remove_all(folder_, ec);
  }
  fs::path folder_;
  Walks walks_;
};

TEST_F(UserOverrideStoreTest, EmptyOrRelativeFolderIsIgnored) {
  const fs::path oldCwd = fs::current_path();
  fs::current_path(folder_);
  {
    UserOverrideStore store("TESTDEV1", "test");
    store.setLearningFolder("");
    store.setLearningFolder("learning");
    store.observe(walks_.oilBefore, walks_.oilAfter, 1, kNow);
    EXPECT_FALSE(store.save());
  }
  EXPECT_TRUE(fs::is_empty(fs::current_path()));
  fs::current_path(oldCwd);
}

TEST_F(UserOverrideStoreTest, MergesOtherDevicesAndSavesOnlyOwnFile) {
  const fs::path macFile = folder_ / "1778D9F7-Mac Studio.txt";
  const std::string macContent = accumulateLine(walks_);
  writeFile(macFile, macContent);

  UserOverrideStore store("AAAA0001", "mint");
  store.setLearningFolder(folder_);
  EXPECT_FALSE(store.isLocalFilePending());
  EXPECT_EQ(store.mergedModel()->suggest(walks_.accFresh, 0, kNow).candidate,
            "積");

  store.observe(walks_.oilBefore, walks_.oilAfter, 1, kNow);
  EXPECT_EQ(store.mergedModel()->suggest(walks_.oilFresh, 1, kNow).candidate,
            "機油");
  ASSERT_TRUE(store.save());

  const fs::path ownFile = folder_ / "AAAA0001-mint.txt";
  ASSERT_TRUE(fs::exists(ownFile));
  EXPECT_EQ(readFile(macFile), macContent);
  const std::string own = readFile(ownFile);
  EXPECT_EQ(own.find("1778D9F7"), std::string::npos);
  EXPECT_EQ(own.find("積"), std::string::npos);
  EXPECT_NE(own.find("機油"), std::string::npos);
}

TEST_F(UserOverrideStoreTest, DoesNotOverwriteBeforeLocalFileLoads) {
  // The user data folder itself is missing, for example a sync folder that
  // has not arrived yet.
  const fs::path dataFolder = folder_ / "data";
  const fs::path notYet = dataFolder / "learning";
  UserOverrideStore store("AAAA0001", "mint");
  store.setLearningFolder(notYet);
  store.observe(walks_.oilBefore, walks_.oilAfter, 1, kNow);
  EXPECT_TRUE(store.isLocalFilePending());
  EXPECT_FALSE(store.save());
  EXPECT_FALSE(fs::exists(notYet / "AAAA0001-mint.txt"));
  EXPECT_FALSE(fs::exists(dataFolder));

  // The folder and the earlier file of this device arrive through syncing.
  fs::create_directories(notYet);
  writeFile(notYet / "AAAA0001-mint.txt", accumulateLine(walks_));
  EXPECT_TRUE(store.reloadIfChanged());
  EXPECT_FALSE(store.isLocalFilePending());
  ASSERT_TRUE(store.save());

  const std::string saved = readFile(notYet / "AAAA0001-mint.txt");
  EXPECT_NE(saved.find("積"), std::string::npos);    // the earlier data stays
  EXPECT_NE(saved.find("機油"), std::string::npos);  // so does the new choice
  EXPECT_EQ(store.mergedModel()->suggest(walks_.accFresh, 0, kNow).candidate,
            "積");

  // The own save does not count as a change in the folder.
  EXPECT_FALSE(store.reloadIfChanged());
}

TEST_F(UserOverrideStoreTest, UnreadableOwnFileStaysPending) {
  // Something that exists under the own file name but cannot be read as a file.
  fs::create_directories(folder_ / "AAAA0001-mint.txt");
  UserOverrideStore store("AAAA0001", "mint");
  store.setLearningFolder(folder_);
  store.observe(walks_.oilBefore, walks_.oilAfter, 1, kNow);
  EXPECT_TRUE(store.isLocalFilePending());
  EXPECT_FALSE(store.save());
  EXPECT_TRUE(fs::is_directory(folder_ / "AAAA0001-mint.txt"));
}

TEST_F(UserOverrideStoreTest, MissingLearningFolderIsCreatedOnSave) {
  // folder_ is the user data folder; learning/ does not exist yet.
  const fs::path learning = folder_ / "learning";
  UserOverrideStore store("AAAA0001", "mint");
  store.setLearningFolder(learning);
  EXPECT_FALSE(store.isLocalFilePending());
  EXPECT_FALSE(fs::exists(learning));
  store.observe(walks_.oilBefore, walks_.oilAfter, 1, kNow);
  ASSERT_TRUE(store.save());
  EXPECT_NE(readFile(learning / "AAAA0001-mint.txt").find("機油"),
            std::string::npos);
  EXPECT_FALSE(store.reloadIfChanged());

  UserOverrideStore other("AAAA0001", "mint");
  other.setLearningFolder(learning);
  EXPECT_FALSE(other.isLocalFilePending());
  EXPECT_EQ(other.mergedModel()->candidateObservationCount("機油"), 1u);
}

TEST_F(UserOverrideStoreTest, LearningFolderBlockedByFileStaysPending) {
  // A regular file where the learning folder should be.
  const fs::path learning = folder_ / "learning";
  writeFile(learning, "synthetic");
  UserOverrideStore store("AAAA0001", "mint");
  store.setLearningFolder(learning);
  store.observe(walks_.oilBefore, walks_.oilAfter, 1, kNow);
  EXPECT_TRUE(store.isLocalFilePending());
  EXPECT_FALSE(store.save());
  EXPECT_EQ(readFile(learning), "synthetic");
}

TEST_F(UserOverrideStoreTest, EmptyFolderLoadsAndSaveCreatesOwnFile) {
  UserOverrideStore store("AAAA0001", "mint");
  store.setLearningFolder(folder_);
  EXPECT_FALSE(store.isLocalFilePending());
  store.observe(walks_.oilBefore, walks_.oilAfter, 1, kNow);
  ASSERT_TRUE(store.save());
  EXPECT_NE(readFile(folder_ / "AAAA0001-mint.txt").find("機油"),
            std::string::npos);
  EXPECT_FALSE(store.reloadIfChanged());
}

TEST_F(UserOverrideStoreTest, SkipsTruncatedLines) {
  std::string good = accumulateLine(walks_);
  ASSERT_FALSE(good.empty());
  std::string firstLine = good.substr(0, good.find('\n'));
  std::string content = firstLine + "\n" + "onlykey\tcandidate\n" + "\n" +
                        firstLine.substr(0, firstLine.size() / 2);
  writeFile(folder_ / "523308DB-MacBook Neo.txt", content);

  UserOverrideStore store("AAAA0001", "mint");
  store.setLearningFolder(folder_);
  store.reload();
  EXPECT_EQ(store.mergedModel()->suggest(walks_.accFresh, 0, kNow).candidate,
            "積");
}

TEST_F(UserOverrideStoreTest, ReloadIfChangedDetectsNewDeviceFile) {
  UserOverrideStore store("AAAA0001", "mint");
  store.setLearningFolder(folder_);
  EXPECT_FALSE(store.reloadIfChanged());
  EXPECT_TRUE(store.mergedModel()->suggest(walks_.accFresh, 0, kNow).empty());

  writeFile(folder_ / "523308DB-MacBook Neo.txt", accumulateLine(walks_));
  EXPECT_TRUE(store.reloadIfChanged());
  EXPECT_EQ(store.mergedModel()->suggest(walks_.accFresh, 0, kNow).candidate,
            "積");
  EXPECT_FALSE(store.reloadIfChanged());
}

TEST_F(UserOverrideStoreTest, IgnoresConflictCopiesAndHiddenFiles) {
  writeFile(folder_ / "523308DB-MacBook Neo_Conflict.txt",
            accumulateLine(walks_));
  writeFile(folder_ / ".523308DB-MacBook Neo.txt", accumulateLine(walks_));
  UserOverrideStore store("AAAA0001", "mint");
  store.setLearningFolder(folder_);
  EXPECT_TRUE(store.mergedModel()->suggest(walks_.accFresh, 0, kNow).empty());
}

TEST_F(UserOverrideStoreTest, AdoptsRenamedOwnFile) {
  const std::string content = accumulateLine(walks_);
  writeFile(folder_ / "AAAA0001-oldname.txt", content);

  UserOverrideStore store("AAAA0001", "mint");
  store.setLearningFolder(folder_);
  EXPECT_FALSE(fs::exists(folder_ / "AAAA0001-oldname.txt"));
  ASSERT_TRUE(fs::exists(folder_ / "AAAA0001-mint.txt"));
  EXPECT_EQ(readFile(folder_ / "AAAA0001-mint.txt"), content);
  EXPECT_EQ(store.mergedModel()->suggest(walks_.accFresh, 0, kNow).candidate,
            "積");
  EXPECT_FALSE(store.reloadIfChanged());
}

TEST_F(UserOverrideStoreTest, SanitizesDeviceName) {
  UserOverrideStore store("AAAA0001", "my/box");
  store.setLearningFolder(folder_);
  store.observe(walks_.oilBefore, walks_.oilAfter, 1, kNow);
  ASSERT_TRUE(store.save());
  EXPECT_TRUE(fs::exists(folder_ / "AAAA0001-my_box.txt"));
}

TEST_F(UserOverrideStoreTest, SettingSameFolderAgainKeepsCounts) {
  UserOverrideStore store("AAAA0001", "mint");
  store.setLearningFolder(folder_);
  store.observe(walks_.oilBefore, walks_.oilAfter, 1, kNow);
  ASSERT_TRUE(store.save());
  const std::string first = readFile(folder_ / "AAAA0001-mint.txt");

  store.setLearningFolder(folder_);
  store.setLearningFolder(folder_ / "." / "");
  EXPECT_FALSE(store.isLocalFilePending());
  EXPECT_EQ(store.mergedModel()->candidateObservationCount("機油"), 1u);
  ASSERT_TRUE(store.save());
  EXPECT_EQ(readFile(folder_ / "AAAA0001-mint.txt"), first);

  // A second store loading the same file sees the same count.
  UserOverrideStore other("AAAA0001", "mint");
  other.setLearningFolder(folder_);
  EXPECT_EQ(other.mergedModel()->candidateObservationCount("機油"), 1u);
}

TEST_F(UserOverrideStoreTest, SwitchingFoldersKeepsEachFolderApart) {
  const fs::path a = folder_ / "a";
  const fs::path b = folder_ / "b";
  fs::create_directories(a);
  fs::create_directories(b);

  UserOverrideStore store("AAAA0001", "mint");
  store.setLearningFolder(a);
  store.observe(walks_.oilBefore, walks_.oilAfter, 1, kNow);
  ASSERT_TRUE(store.save());
  const std::string aContent = readFile(a / "AAAA0001-mint.txt");

  store.setLearningFolder(b);
  EXPECT_FALSE(store.isLocalFilePending());
  EXPECT_EQ(store.mergedModel()->candidateObservationCount("機油"), 0u);
  store.observe(walks_.accBefore, walks_.accAfter, 0, kNow);
  ASSERT_TRUE(store.save());
  const std::string bContent = readFile(b / "AAAA0001-mint.txt");
  EXPECT_EQ(bContent.find("機油"), std::string::npos);
  EXPECT_NE(bContent.find("積"), std::string::npos);

  store.setLearningFolder(a);
  EXPECT_EQ(store.mergedModel()->candidateObservationCount("機油"), 1u);
  EXPECT_EQ(store.mergedModel()->candidateObservationCount("積"), 0u);
  ASSERT_TRUE(store.save());
  EXPECT_EQ(readFile(a / "AAAA0001-mint.txt"), aContent);
  EXPECT_EQ(readFile(b / "AAAA0001-mint.txt"), bContent);
}

TEST_F(UserOverrideStoreTest, SwitchingFoldersSavesUnsavedChoicesToOldFolder) {
  const fs::path a = folder_ / "a";
  const fs::path b = folder_ / "b";
  fs::create_directories(a);
  fs::create_directories(b);

  UserOverrideStore store("AAAA0001", "mint");
  store.setLearningFolder(a);
  store.observe(walks_.oilBefore, walks_.oilAfter, 1, kNow);
  store.setLearningFolder(b);
  EXPECT_NE(readFile(a / "AAAA0001-mint.txt").find("機油"), std::string::npos);
}

}  // namespace McBopomofo::SmartLearning
