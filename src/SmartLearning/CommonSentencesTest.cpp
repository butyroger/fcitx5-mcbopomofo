#include "CommonSentences.h"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "LearningPaths.h"

namespace McBopomofo::SmartLearning {
namespace {

namespace fs = std::filesystem;

constexpr double kNow = 1700000000;
constexpr double kDay = 86400;
constexpr char kSentence[] = "謝謝您的耐心等候";

void writeFile(const fs::path& path, const std::string& content) {
  fs::create_directories(path.parent_path());
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  out << content;
}

std::string readFile(const fs::path& path) {
  std::ifstream in(path, std::ios::binary);
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

std::string line(const std::string& sentence, long long count, double time,
                 bool pinned = false) {
  return sentence + "\t" + std::to_string(count) + "\t" +
         std::to_string(static_cast<long long>(time)) + "\t" +
         (pinned ? "1" : "0") + "\n";
}

TEST(SentenceLengthTest, WordsAndNumbersCountOne) {
  EXPECT_EQ(SentenceLength(kSentence), 8u);
  EXPECT_EQ(SentenceLength("ChatGPT 很好用的工具"), 7u);
  EXPECT_EQ(SentenceLength("hello 你好"), 3u);
  EXPECT_EQ(SentenceLength("2026年11月"), 4u);
  EXPECT_EQ(SentenceLength("a1b2 c3"), 2u);
  EXPECT_EQ(SentenceLength("  "), 0u);
  // A character made of several code points counts once.
  EXPECT_EQ(SentenceLength("e\xCC\x81"), 1u);
  EXPECT_EQ(SentenceLength(""), 0u);
}

TEST(SentenceLengthTest, Validity) {
  EXPECT_FALSE(IsValidSentence("謝謝您的耐"));    // 5
  EXPECT_TRUE(IsValidSentence("謝謝您的耐心"));   // 6
  EXPECT_TRUE(IsValidSentence("ChatGPT 很好用的工具"));
  std::string forty;
  for (int i = 0; i < 40; i++) {
    forty += "字";
  }
  EXPECT_TRUE(IsValidSentence(forty));
  EXPECT_FALSE(IsValidSentence(forty + "字"));  // 41
  EXPECT_FALSE(IsValidSentence("謝謝您的，耐心等候"));
  EXPECT_FALSE(IsValidSentence("謝謝您的耐心等候😀"));
  EXPECT_FALSE(IsValidSentence("謝謝您的\n耐心等候"));
}

TEST(SplitSentencesTest, PunctuationAndEmojiSplitButSpacesDoNot) {
  EXPECT_EQ(SplitSentences("謝謝您的耐心等候，請您再稍等一下喔！"),
            (std::vector<std::string>{kSentence, "請您再稍等一下喔"}));
  EXPECT_EQ(SplitSentences("今天天氣真的很好😀明天也會很好的"),
            (std::vector<std::string>{"今天天氣真的很好", "明天也會很好的"}));
  EXPECT_EQ(SplitSentences("ChatGPT 很好用的工具"),
            (std::vector<std::string>{"ChatGPT 很好用的工具"}));
  // Trimmed at both ends; too short parts are left out.
  EXPECT_EQ(SplitSentences("  謝謝您的耐心等候  。好的\n明天見面再聊吧\t嗯"),
            (std::vector<std::string>{kSentence, "明天見面再聊吧"}));
  // Symbols such as "+" and "$" split too.
  EXPECT_EQ(SplitSentences("一二三四五六+七八九十百千$"),
            (std::vector<std::string>{"一二三四五六", "七八九十百千"}));
  EXPECT_TRUE(SplitSentences("好的。").empty());
  EXPECT_TRUE(SplitSentences("").empty());
}

class CommonSentencesTest : public ::testing::Test {
 protected:
  void SetUp() override {
    auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    root_ = fs::temp_directory_path() /
            ("smartlearning-sentences-" + std::to_string(stamp) + "-" +
             ::testing::UnitTest::GetInstance()->current_test_info()->name());
    learning_ = root_ / "learning";
    sentences_ = learning_ / "sentences";
    fs::create_directories(root_);
  }
  void TearDown() override {
    std::error_code ec;
    fs::remove_all(root_, ec);
  }

  std::unique_ptr<CommonSentenceStore> makeStore() {
    removed_ = std::make_shared<RemovedEntries>([this] { return now_; });
    return std::make_unique<CommonSentenceStore>(
        "TESTDEV1", "test", [this] { return now_; }, removed_);
  }

  std::unique_ptr<CommonSentenceStore> loadedStore() {
    auto store = makeStore();
    store->setLearningFolder(learning_);
    return store;
  }

  fs::path ownFile() const {
    return sentences_ / learningFilename("TESTDEV1", "test");
  }

  fs::path root_;
  fs::path learning_;
  fs::path sentences_;
  double now_ = kNow;
  std::shared_ptr<RemovedEntries> removed_;
};

TEST_F(CommonSentencesTest, SuggestedAfterThreeCommits) {
  auto store = loadedStore();
  store->record(kSentence);
  store->record(kSentence);
  EXPECT_EQ(store->completion("謝謝"), std::nullopt);
  store->record(kSentence);
  EXPECT_EQ(store->completion("謝謝"), "您的耐心等候");
  EXPECT_EQ(store->completion("謝謝您的耐心等"), "候");
  // One character is not enough, and the whole sentence leaves nothing.
  EXPECT_EQ(store->completion("謝"), std::nullopt);
  EXPECT_EQ(store->completion(kSentence), std::nullopt);
  EXPECT_EQ(store->completion("謝了"), std::nullopt);
}

TEST_F(CommonSentencesTest, CompletionLooksAfterTheLastSeparator) {
  auto store = loadedStore();
  for (int i = 0; i < 3; i++) {
    store->record(kSentence);
  }
  EXPECT_EQ(store->completion("好的，謝謝"), "您的耐心等候");
  EXPECT_EQ(store->completion("好的， 謝謝"), "您的耐心等候");
  EXPECT_EQ(store->completion("好的😀謝謝"), "您的耐心等候");
  // Spaces do not separate.
  EXPECT_EQ(store->completion("好的 謝謝"), std::nullopt);
  EXPECT_EQ(store->completion("好的，謝"), std::nullopt);
}

TEST_F(CommonSentencesTest, ShortAndPunctuatedSentences) {
  auto store = loadedStore();
  for (int i = 0; i < 3; i++) {
    store->record("謝謝您的耐");  // five characters
    store->record("明天見面再聊吧，謝謝您的耐心等候");
  }
  EXPECT_EQ(store->completion("謝謝"), "您的耐心等候");
  EXPECT_EQ(store->completion("明天"), "見面再聊吧");
  EXPECT_FALSE(store->entry("謝謝您的耐").has_value());
}

TEST_F(CommonSentencesTest, EnglishWordsInSentences) {
  auto store = loadedStore();
  for (int i = 0; i < 3; i++) {
    store->record("ChatGPT 很好用的工具");
  }
  EXPECT_EQ(store->completion("ChatGPT"), " 很好用的工具");
  EXPECT_EQ(store->completion("Ch"), "atGPT 很好用的工具");
}

TEST_F(CommonSentencesTest, MostUsedThenLatestWins) {
  auto store = loadedStore();
  for (int i = 0; i < 4; i++) {
    store->record("謝謝大家的熱情參與");
  }
  for (int i = 0; i < 3; i++) {
    store->record(kSentence);
  }
  EXPECT_EQ(store->completion("謝謝"), "大家的熱情參與");
  store->record(kSentence);
  now_ += 10;
  store->record(kSentence);
  EXPECT_EQ(store->completion("謝謝"), "您的耐心等候");

  now_ += 10;
  for (int i = 0; i < 5; i++) {
    store->record("謝謝各位今天的參與");
  }
  // Five each: the one used last wins.
  EXPECT_EQ(store->completion("謝謝"), "各位今天的參與");
}

TEST_F(CommonSentencesTest, DisabledRecordsAndSuggestsNothing) {
  auto store = loadedStore();
  for (int i = 0; i < 3; i++) {
    store->record(kSentence);
  }
  store->setEnabled(false);
  EXPECT_EQ(store->completion("謝謝"), std::nullopt);
  store->record("明天見面再聊吧");
  store->setEnabled(true);
  EXPECT_FALSE(store->entry("明天見面再聊吧").has_value());
  EXPECT_EQ(store->completion("謝謝"), "您的耐心等候");
}

TEST_F(CommonSentencesTest, Lifetime) {
  using Entry = CommonSentenceStore::Entry;
  EXPECT_EQ(CommonSentenceStore::lifetime(Entry{3, 0, false}), 90 * kDay);
  EXPECT_EQ(CommonSentenceStore::lifetime(Entry{5, 0, false}), 90 * kDay);
  EXPECT_EQ(CommonSentenceStore::lifetime(Entry{6, 0, false}), 120 * kDay);
  EXPECT_EQ(CommonSentenceStore::lifetime(Entry{9, 0, false}), 150 * kDay);
  EXPECT_EQ(CommonSentenceStore::lifetime(Entry{1, 0, false}), 90 * kDay);
}

TEST_F(CommonSentencesTest, ExpiredSentencesAreNotSuggestedNorSaved) {
  writeFile(ownFile(), line("一二三四五六", 3, kNow - 90 * kDay) +
                           line("七六五四三二", 3, kNow - 90 * kDay - 1) +
                           line("甲乙丙丁戊己", 9, kNow - 150 * kDay) +
                           line("子丑寅卯辰巳", 9, kNow - 150 * kDay - 1) +
                           line("東南西北中發", 1, kNow - 400 * kDay, true));
  auto store = loadedStore();
  EXPECT_EQ(store->completion("一二"), "三四五六");
  EXPECT_EQ(store->completion("七六"), std::nullopt);
  // Nine times: 150 days.
  EXPECT_EQ(store->completion("甲乙"), "丙丁戊己");
  EXPECT_EQ(store->completion("子丑"), std::nullopt);
  // Added by the user: never forgotten, and remembered whatever its count.
  EXPECT_EQ(store->completion("東南"), "西北中發");

  // Saving drops the expired ones.
  store->record("明天見面再聊吧");
  ASSERT_TRUE(store->save());
  EXPECT_EQ(readFile(ownFile()),
            line("一二三四五六", 3, kNow - 90 * kDay) +
                line("明天見面再聊吧", 1, kNow) +
                line("東南西北中發", 1, kNow - 400 * kDay, true) +
                line("甲乙丙丁戊己", 9, kNow - 150 * kDay));
}

TEST_F(CommonSentencesTest, RemovedOnAnotherDevice) {
  writeFile(sentences_ / "AAAA0002-laptop.txt", line(kSentence, 5, kNow - 100));
  writeFile(learning_ / "removed.txt",
            std::string("sentence\t") + kSentence + "\t" +
                std::to_string(static_cast<long long>(kNow - 100)) + "\n");
  auto store = loadedStore();
  EXPECT_EQ(store->completion("謝謝"), std::nullopt);

  // Used again after it was removed, it comes back.
  for (int i = 0; i < 3; i++) {
    store->record(kSentence);
  }
  EXPECT_EQ(store->completion("謝謝"), "您的耐心等候");

  // Removed before it was used last: still suggested.
  writeFile(learning_ / "removed.txt",
            std::string("sentence\t") + kSentence + "\t" +
                std::to_string(static_cast<long long>(kNow - 101)) + "\n" +
                "english\t明天見面再聊吧\t" +
                std::to_string(static_cast<long long>(kNow)) + "\n");
  writeFile(sentences_ / "AAAA0002-laptop.txt",
            line(kSentence, 5, kNow - 100) + line("明天見面再聊吧", 3, kNow));
  auto other = loadedStore();
  EXPECT_EQ(other->completion("謝謝"), "您的耐心等候");
  // Another kind of entry does not count.
  EXPECT_EQ(other->completion("明天"), "見面再聊吧");
}

TEST_F(CommonSentencesTest, DevicesAreMerged) {
  writeFile(ownFile(), line(kSentence, 1, kNow - 50));
  writeFile(sentences_ / "AAAA0002-laptop.txt",
            line(kSentence, 2, kNow - 10) + line("明天見面再聊吧", 1, kNow, true));
  // Conflict copies and hidden files are not other devices.
  writeFile(sentences_ / "AAAA0003-desk_Conflict.txt",
            line("今天天氣真的很好", 5, kNow));
  writeFile(sentences_ / ".hidden.txt", line("今天天氣真的很好", 5, kNow));
  auto store = loadedStore();
  auto merged = store->entry(kSentence);
  ASSERT_TRUE(merged.has_value());
  EXPECT_EQ(merged->count, 3);
  EXPECT_EQ(merged->lastUsed, kNow - 10);
  EXPECT_EQ(store->completion("謝謝"), "您的耐心等候");
  EXPECT_EQ(store->completion("明天"), "見面再聊吧");
  EXPECT_EQ(store->completion("今天"), std::nullopt);

  // Only the own sentences are written.
  store->record(kSentence);
  ASSERT_TRUE(store->save());
  EXPECT_EQ(readFile(ownFile()), line(kSentence, 2, kNow));
  EXPECT_EQ(store->entry(kSentence)->count, 4);
}

TEST_F(CommonSentencesTest, FileParsing) {
  writeFile(ownFile(),
            "一二三四五六七\t3\t1699999000\n"         // no pinned field
            "一二三四五六八\t3\n"                     // two fields
            "一二三四五六九\tx\t1699999000\t0\n"      // bad count
            "一二三四五六十\t3\tlater\t0\n"           // bad time
            "七六五四三二一\t1\t1699999000.5\t1\textra\n"  // pinned
            "\n"
            "七七七七七七七\t1\t1699999000\t2\n");    // not pinned
  auto store = loadedStore();
  ASSERT_TRUE(store->entry("一二三四五六七").has_value());
  EXPECT_FALSE(store->entry("一二三四五六七")->pinned);
  EXPECT_FALSE(store->entry("一二三四五六八").has_value());
  EXPECT_FALSE(store->entry("一二三四五六九").has_value());
  EXPECT_FALSE(store->entry("一二三四五六十").has_value());
  ASSERT_TRUE(store->entry("七六五四三二一").has_value());
  EXPECT_TRUE(store->entry("七六五四三二一")->pinned);
  EXPECT_FALSE(store->entry("七七七七七七七")->pinned);
  ASSERT_TRUE(store->save());
  EXPECT_EQ(readFile(ownFile()),
            "一二三四五六七\t3\t1699999000\t0\n"
            "七七七七七七七\t1\t1699999000\t0\n"
            "七六五四三二一\t1\t1699999000\t1\n");
}

TEST_F(CommonSentencesTest, NothingWrittenUntilRecorded) {
  auto store = loadedStore();
  EXPECT_FALSE(store->hasUnsavedChanges());
  store->record("好的。");
  EXPECT_FALSE(store->hasUnsavedChanges());
  store->record(kSentence);
  EXPECT_TRUE(store->hasUnsavedChanges());
  // Recording only touches memory.
  EXPECT_FALSE(fs::exists(sentences_));
  ASSERT_TRUE(store->save());
  EXPECT_FALSE(store->hasUnsavedChanges());
  EXPECT_EQ(readFile(ownFile()), line(kSentence, 1, kNow));
}

TEST_F(CommonSentencesTest, MissingSentencesFolderIsCreatedOnSave) {
  fs::create_directories(learning_);
  auto store = loadedStore();
  EXPECT_FALSE(store->isLocalFilePending());
  store->record(kSentence);
  ASSERT_TRUE(store->save());
  EXPECT_EQ(readFile(ownFile()), line(kSentence, 1, kNow));
}

TEST_F(CommonSentencesTest, PendingUntilTheUserDataFolderExists) {
  // The user data folder itself is missing, e.g. not synced yet.
  fs::remove_all(root_);
  auto store = loadedStore();
  EXPECT_TRUE(store->isLocalFilePending());
  store->record(kSentence);
  store->record(kSentence);
  EXPECT_FALSE(store->save());
  EXPECT_FALSE(fs::exists(root_));
  EXPECT_TRUE(store->hasUnsavedChanges());

  // The folder arrives with this device's file: what was recorded meanwhile
  // is added to it.
  writeFile(ownFile(), line(kSentence, 2, kNow - 100) +
                           line("明天見面再聊吧", 3, kNow - 100));
  now_ += 1;
  ASSERT_TRUE(store->save());
  EXPECT_FALSE(store->isLocalFilePending());
  EXPECT_EQ(readFile(ownFile()), line("明天見面再聊吧", 3, kNow - 100) +
                                     line(kSentence, 4, kNow));
  EXPECT_EQ(store->completion("謝謝"), "您的耐心等候");
}

TEST_F(CommonSentencesTest, UnreadableOwnFileStaysPending) {
  fs::create_directories(ownFile());  // a folder where the file should be
  auto store = loadedStore();
  EXPECT_TRUE(store->isLocalFilePending());
  store->record(kSentence);
  EXPECT_FALSE(store->save());
  EXPECT_TRUE(fs::is_directory(ownFile()));
}

TEST_F(CommonSentencesTest, OwnFileReadOnlyOnce) {
  writeFile(ownFile(), line(kSentence, 3, kNow));
  auto store = loadedStore();
  writeFile(ownFile(), "");
  now_ += 120;
  store->reloadIfNeeded();
  EXPECT_EQ(store->completion("謝謝"), "您的耐心等候");
  // Setting the same folder again does not add the file's counts again.
  store->setLearningFolder(learning_);
  EXPECT_EQ(store->entry(kSentence)->count, 3);
}

TEST_F(CommonSentencesTest, OtherDevicesReloadedAfterTheInterval) {
  fs::create_directories(sentences_);
  auto store = loadedStore();
  writeFile(sentences_ / "AAAA0002-laptop.txt", line(kSentence, 3, kNow));
  now_ = kNow + 59;
  store->reloadIfNeeded();
  EXPECT_EQ(store->completion("謝謝"), std::nullopt);
  now_ = kNow + 60;
  store->reloadIfNeeded();
  EXPECT_EQ(store->completion("謝謝"), "您的耐心等候");
}

TEST_F(CommonSentencesTest, UnchangedOtherDevicesAreNotReadAgain) {
  fs::create_directories(sentences_);
  const fs::path other = sentences_ / "AAAA0002-laptop.txt";
  writeFile(other, line(kSentence, 3, kNow));
  auto store = loadedStore();
  ASSERT_EQ(store->completion("謝謝"), "您的耐心等候");

  // Same size and modification time: taken as unchanged, so not read.
  const auto mtime = fs::last_write_time(other);
  writeFile(other, line("謝謝您的耐心等待", 3, kNow));
  fs::last_write_time(other, mtime);
  now_ = kNow + 60;
  store->reloadIfNeeded();
  EXPECT_EQ(store->completion("謝謝"), "您的耐心等候");

  // A change of size is read.
  writeFile(other, line("謝謝您的耐心與包容", 3, kNow));
  now_ = kNow + 120;
  store->reloadIfNeeded();
  EXPECT_EQ(store->completion("謝謝"), "您的耐心與包容");
}

TEST_F(CommonSentencesTest, SwitchingFoldersKeepsOwnSentencesInTheOldOne) {
  auto store = loadedStore();
  store->record(kSentence);
  fs::path otherRoot = root_ / "other";
  fs::create_directories(otherRoot / "learning");
  store->setLearningFolder(otherRoot / "learning");
  EXPECT_EQ(readFile(ownFile()), line(kSentence, 1, kNow));
  EXPECT_FALSE(store->entry(kSentence).has_value());
}

// MARK: - Vocabulary manager

std::string removedLine(const std::string& sentence, double time) {
  return "sentence\t" + sentence + "\t" +
         std::to_string(static_cast<long long>(time)) + "\n";
}

TEST_F(CommonSentencesTest, AllEntriesListsTheLiveOnesMostUsedFirst) {
  writeFile(ownFile(), line("一二三四五六", 2, kNow - 10) +
                           line("七六五四三二", 5, kNow - 20) +
                           line("東南西北中發", 1, kNow - 400 * kDay, true) +
                           line("甲乙丙丁戊己", 3, kNow - 100 * kDay) +
                           line("子丑寅卯辰巳", 9, kNow - 30));
  writeFile(sentences_ / "AAAA0002-laptop.txt",
            line("一二三四五六", 1, kNow - 5) + line("明天見面再聊吧", 5, kNow));
  writeFile(learning_ / "removed.txt", removedLine("子丑寅卯辰巳", kNow - 30));
  auto store = loadedStore();
  store->setEnabled(false);  // listed even when turned off
  auto entries = store->allEntries();
  ASSERT_EQ(entries.size(), 4U);
  EXPECT_EQ(entries[0].sentence, "明天見面再聊吧");  // 5, newer
  EXPECT_EQ(entries[1].sentence, "七六五四三二");    // 5, older
  EXPECT_EQ(entries[2].sentence, "一二三四五六");    // 2 + 1
  EXPECT_EQ(entries[2].entry.count, 3);
  EXPECT_EQ(entries[2].entry.lastUsed, kNow - 5);
  EXPECT_EQ(entries[3].sentence, "東南西北中發");
  EXPECT_TRUE(entries[3].entry.pinned);
}

TEST_F(CommonSentencesTest, AddPinsAndRemembersAtOnce) {
  writeFile(ownFile(), line("明天見面再聊吧", 5, kNow - 100));
  writeFile(learning_ / "removed.txt",
            removedLine(kSentence, kNow - 50) +
                removedLine("一二三四五六", kNow - 50));
  auto store = loadedStore();
  now_ += 1;
  EXPECT_FALSE(store->add("太短了"));
  EXPECT_FALSE(store->add("有標點，不行的句子"));
  ASSERT_TRUE(store->add(kSentence));
  ASSERT_TRUE(store->add("明天見面再聊吧"));
  EXPECT_EQ(readFile(ownFile()), line("明天見面再聊吧", 5, now_, true) +
                                     line(kSentence, 3, now_, true));
  EXPECT_EQ(readFile(learning_ / "removed.txt"),
            removedLine("一二三四五六", kNow - 50));
  EXPECT_EQ(store->completion("謝謝"), "您的耐心等候");
  EXPECT_FALSE(store->hasUnsavedChanges());
}

TEST_F(CommonSentencesTest, RemoveRecordsTheRemovalAndForgetsTheOwnCount) {
  writeFile(ownFile(),
            line(kSentence, 4, kNow - 100) + line("明天見面再聊吧", 3, kNow));
  writeFile(sentences_ / "AAAA0002-laptop.txt", line(kSentence, 2, kNow - 90));
  auto store = loadedStore();
  now_ += 10;
  ASSERT_TRUE(store->remove(kSentence));
  EXPECT_EQ(readFile(ownFile()), line("明天見面再聊吧", 3, kNow));
  EXPECT_EQ(readFile(learning_ / "removed.txt"), removedLine(kSentence, now_));
  EXPECT_EQ(readFile(sentences_ / "AAAA0002-laptop.txt"),
            line(kSentence, 2, kNow - 90));
  EXPECT_EQ(store->allEntries().size(), 1U);
  EXPECT_EQ(store->completion("謝謝"), std::nullopt);

  // Used again, it starts over from the other device's count.
  now_ += 1;
  store->record(kSentence);
  EXPECT_EQ(store->entry(kSentence)->count, 3);
}

TEST_F(CommonSentencesTest, ManagementWritesNothingWhilePending) {
  fs::remove_all(root_);
  auto store = loadedStore();
  ASSERT_TRUE(store->isLocalFilePending());
  EXPECT_FALSE(store->add(kSentence));
  EXPECT_FALSE(store->remove(kSentence));
  EXPECT_FALSE(fs::exists(root_));
  EXPECT_TRUE(store->allEntries().empty());
}

TEST_F(CommonSentencesTest, ASentenceRemovedByAnotherProgramStartsOver) {
  writeFile(ownFile(), line(kSentence, 3, kNow - 100));
  auto ime = loadedStore();
  auto manager = loadedStore();
  now_ += 10;
  ASSERT_TRUE(manager->remove(kSentence));

  // The input method is told to check its files again, then the sentence is
  // used again: its count starts from 1.
  ime->reload();
  EXPECT_EQ(ime->completion("謝謝"), std::nullopt);
  now_ += 1;
  ime->record(kSentence);
  EXPECT_EQ(ime->entry(kSentence)->count, 1);
  ASSERT_TRUE(ime->save());
  EXPECT_EQ(readFile(ownFile()), line(kSentence, 1, now_));
}

TEST_F(CommonSentencesTest, ASentenceAddedByAnotherProgramSurvivesTheNextSave) {
  writeFile(ownFile(), line(kSentence, 1, kNow - 100));
  auto ime = loadedStore();
  auto manager = loadedStore();
  now_ += 10;
  ASSERT_TRUE(manager->add("明天見面再聊吧"));
  const double added = now_;

  // The input method saves what it recorded since, without having reloaded.
  now_ += 1;
  ime->record(kSentence);
  ASSERT_TRUE(ime->save());
  EXPECT_EQ(readFile(ownFile()), line("明天見面再聊吧", 3, added, true) +
                                     line(kSentence, 2, now_));
  EXPECT_EQ(ime->completion("明天"), "見面再聊吧");

  // Counts take the larger, the later time wins and pinned stays. (A file
  // of the same size written within the same clock tick would be taken as
  // unchanged.)
  writeFile(ownFile(), line(kSentence, 1, kNow - 200) +
                           line("明天見面再聊吧", 17, kNow - 200));
  now_ = kNow + 120;
  ime->reloadIfNeeded();
  auto entry = ime->entry("明天見面再聊吧");
  ASSERT_TRUE(entry.has_value());
  EXPECT_EQ(entry->count, 17);
  EXPECT_EQ(entry->lastUsed, added);
  EXPECT_TRUE(entry->pinned);
  EXPECT_EQ(ime->entry(kSentence)->count, 2);
}

}  // namespace
}  // namespace McBopomofo::SmartLearning
