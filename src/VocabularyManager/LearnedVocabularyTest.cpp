#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "LearnedVocabulary.h"
#include "LearningPaths.h"
#include "ManagerSettings.h"
#include "TemporaryPhrases.h"

namespace McBopomofo::VocabularyManager {
namespace fs = std::filesystem;

namespace {

constexpr char kDeviceID[] = "AAAA1111";
constexpr char kHostName[] = "testhost";
// A fixed Unix time, at noon UTC so that the local date is the same in most
// time zones.
constexpr double kNow = 1767268800;  // 2026-01-01 12:00 UTC
constexpr double kDay = 86400;

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

SentenceStatusTexts statusTexts() {
  return {"added", "remembered",
          [](long long n) { return std::to_string(n) + " more"; }};
}

std::vector<std::vector<std::string>> columnsOf(
    const std::vector<VocabularyRow>& rows) {
  std::vector<std::vector<std::string>> columns;
  for (const auto& row : rows) {
    columns.push_back(row.columns);
  }
  return columns;
}

std::vector<std::string> keysOf(const std::vector<VocabularyRow>& rows) {
  std::vector<std::string> keys;
  for (const auto& row : rows) {
    keys.push_back(row.key);
  }
  return keys;
}

class LearnedVocabularyTest : public ::testing::Test {
 protected:
  void SetUp() override {
    auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    folder_ = fs::temp_directory_path() /
              ("learned-vocabulary-" + std::to_string(stamp) + "-" +
               ::testing::UnitTest::GetInstance()->current_test_info()->name());
    fs::create_directories(folder_);
    now_ = kNow;
  }
  void TearDown() override {
    std::error_code ec;
    fs::remove_all(folder_, ec);
  }

  std::unique_ptr<LearnedVocabulary> make(const std::string& deviceID) {
    return std::make_unique<LearnedVocabulary>(folder_, deviceID, kHostName,
                                               [this] { return now_; });
  }

  fs::path learning() const { return folder_ / "learning"; }
  fs::path own(const char* kind) const {
    return learning() / kind /
           SmartLearning::learningFilename(kDeviceID, kHostName);
  }
  fs::path other(const char* kind) const {
    return learning() / kind / "BBBB2222-other.txt";
  }
  fs::path removed() const { return learning() / "removed.txt"; }

  fs::path folder_;
  double now_ = 0;
};

}  // namespace

// MARK: Validation and dates

TEST(LearnedVocabularyHelpersTest, AddableEnglishWords) {
  EXPECT_TRUE(isAddableEnglishWord("ok"));
  EXPECT_TRUE(isAddableEnglishWord("iPhone"));
  EXPECT_FALSE(isAddableEnglishWord(""));
  EXPECT_FALSE(isAddableEnglishWord("a"));
  EXPECT_FALSE(isAddableEnglishWord("ab1"));
  EXPECT_FALSE(isAddableEnglishWord("two words"));
  EXPECT_FALSE(isAddableEnglishWord("café"));
  EXPECT_FALSE(isAddableEnglishWord("中文"));
}

TEST(LearnedVocabularyHelpersTest, LocalDate) {
  EXPECT_EQ(localDate(kNow), "2026-01-01");
  EXPECT_EQ(localDate(kNow + 31 * kDay), "2026-02-01");
}

// MARK: English words

TEST_F(LearnedVocabularyTest, EnglishWordsOfAllDevicesSorted) {
  writeFile(own("english"), "zebra\t1\nApple\t2\n");
  writeFile(other("english"), "apple\t3\nzebra\t4\n");
  writeFile(removed(), "english\tApple\t100\n");
  auto vocabulary = make(kDeviceID);
  auto rows = vocabulary->rows(LearnedKind::kEnglish, statusTexts());
  EXPECT_EQ(columnsOf(rows),
            (std::vector<std::vector<std::string>>{{"apple", "3"},
                                                   {"zebra", "5"}}));
  EXPECT_EQ(keysOf(rows), (std::vector<std::string>{"apple", "zebra"}));
}

TEST_F(LearnedVocabularyTest, AddEnglishWord) {
  fs::create_directories(learning());
  writeFile(removed(), "english\tokay\t100\n");
  auto vocabulary = make(kDeviceID);
  auto result = vocabulary->add(LearnedKind::kEnglish, "  okay ");
  EXPECT_EQ(result.status, CommitStatus::kAdded);
  EXPECT_EQ(result.value, "okay");
  EXPECT_EQ(readFile(own("english")), "okay\t2\n");
  EXPECT_EQ(readFile(removed()).find("okay"), std::string::npos);
  EXPECT_EQ(columnsOf(vocabulary->rows(LearnedKind::kEnglish, statusTexts())),
            (std::vector<std::vector<std::string>>{{"okay", "2"}}));
}

TEST_F(LearnedVocabularyTest, AddEnglishWordRefused) {
  fs::create_directories(learning());
  auto vocabulary = make(kDeviceID);
  EXPECT_EQ(vocabulary->add(LearnedKind::kEnglish, " ").status,
            CommitStatus::kNothingTyped);
  EXPECT_EQ(vocabulary->add(LearnedKind::kEnglish, "a").status,
            CommitStatus::kBadWord);
  EXPECT_EQ(vocabulary->add(LearnedKind::kEnglish, "R2D2").status,
            CommitStatus::kBadWord);
  EXPECT_FALSE(fs::exists(learning() / "english"));
}

TEST_F(LearnedVocabularyTest, RemoveEnglishWords) {
  writeFile(own("english"), "apple\t3\nkiwi\t2\n");
  writeFile(other("english"), "apple\t3\n");
  auto vocabulary = make(kDeviceID);
  auto result = vocabulary->remove(LearnedKind::kEnglish, {"apple"});
  EXPECT_EQ(result.status, DeleteStatus::kDeleted);
  EXPECT_EQ(result.done, 1u);
  EXPECT_EQ(readFile(own("english")), "kiwi\t2\n");
  // The other device's file is left as it is; removed.txt hides the word.
  EXPECT_EQ(readFile(other("english")), "apple\t3\n");
  EXPECT_EQ(readFile(removed()),
            "english\tapple\t" + std::to_string(static_cast<long long>(kNow)) +
                "\n");
  EXPECT_EQ(keysOf(vocabulary->rows(LearnedKind::kEnglish, statusTexts())),
            (std::vector<std::string>{"kiwi"}));
  EXPECT_EQ(vocabulary->remove(LearnedKind::kEnglish, {}).status,
            DeleteStatus::kNothingSelected);
}

// MARK: Common sentences

TEST_F(LearnedVocabularyTest, SentencesWithTheirStatus) {
  std::string t = std::to_string(static_cast<long long>(kNow - kDay));
  writeFile(own("sentences"),
            "我們明天早上見面\t1\t" + t + "\t0\n" + "今天天氣真的很好\t1\t" + t +
                "\t1\n");
  writeFile(other("sentences"), "我們明天早上見面\t1\t" + t + "\t0\n" +
                                    "謝謝你的幫忙喔\t4\t" + t + "\t0\n");
  auto vocabulary = make(kDeviceID);
  auto rows = vocabulary->rows(LearnedKind::kSentences, statusTexts());
  EXPECT_EQ(columnsOf(rows), (std::vector<std::vector<std::string>>{
                                 {"謝謝你的幫忙喔", "4", "remembered"},
                                 {"我們明天早上見面", "2", "1 more"},
                                 {"今天天氣真的很好", "1", "added"}}));
  EXPECT_EQ(rows[1].key, "我們明天早上見面");
}

TEST_F(LearnedVocabularyTest, AddSentence) {
  fs::create_directories(learning());
  auto vocabulary = make(kDeviceID);
  auto result = vocabulary->add(LearnedKind::kSentences, " 下午三點開會 ");
  EXPECT_EQ(result.status, CommitStatus::kAdded);
  EXPECT_EQ(result.value, "下午三點開會");
  EXPECT_EQ(columnsOf(vocabulary->rows(LearnedKind::kSentences, statusTexts())),
            (std::vector<std::vector<std::string>>{
                {"下午三點開會", "3", "added"}}));
  EXPECT_NE(readFile(own("sentences")).find("下午三點開會\t3\t"),
            std::string::npos);
  EXPECT_EQ(vocabulary->add(LearnedKind::kSentences, "太短了").status,
            CommitStatus::kBadSentence);
  EXPECT_EQ(vocabulary->add(LearnedKind::kSentences, "下午三點，開會吧").status,
            CommitStatus::kBadSentence);
}

TEST_F(LearnedVocabularyTest, RemoveSentence) {
  std::string t = std::to_string(static_cast<long long>(kNow - kDay));
  writeFile(own("sentences"), "下午三點開會\t3\t" + t + "\t0\n");
  auto vocabulary = make(kDeviceID);
  auto result = vocabulary->remove(LearnedKind::kSentences, {"下午三點開會"});
  EXPECT_EQ(result.status, DeleteStatus::kDeleted);
  EXPECT_TRUE(
      vocabulary->rows(LearnedKind::kSentences, statusTexts()).empty());
  EXPECT_NE(readFile(removed()).find("sentence\t下午三點開會\t"),
            std::string::npos);
  EXPECT_EQ(readFile(own("sentences")).find("下午三點開會"), std::string::npos);
}

// MARK: Temporary phrases

TEST_F(LearnedVocabularyTest, TemporaryPhrasesNewestFirst) {
  auto at = [](double t) { return std::to_string(static_cast<long long>(t)); };
  writeFile(own("temporary"), "測試 ㄘㄜˋ-ㄕˋ " + at(kNow - 2 * kDay) + "\n");
  writeFile(other("temporary"), "小麥 ㄒㄧㄠˇ-ㄇㄞˋ " + at(kNow - kDay) + "\n" +
                                    "舊詞 ㄐㄧㄡˋ-ㄘˊ " + at(kNow - 40 * kDay) +
                                    "\n");
  auto vocabulary = make(kDeviceID);
  auto rows = vocabulary->rows(LearnedKind::kTemporary, statusTexts());
  EXPECT_EQ(columnsOf(rows),
            (std::vector<std::vector<std::string>>{
                {"小麥", "ㄒㄧㄠˇ-ㄇㄞˋ", localDate(kNow - kDay)},
                {"測試", "ㄘㄜˋ-ㄕˋ", localDate(kNow - 2 * kDay)}}));
  EXPECT_EQ(rows[0].key, "小麥 ㄒㄧㄠˇ-ㄇㄞˋ");
  EXPECT_EQ(vocabulary->add(LearnedKind::kTemporary, "小麥").status,
            CommitStatus::kNothingTyped);
}

TEST_F(LearnedVocabularyTest, ForgetTemporaryPhrase) {
  auto at = std::to_string(static_cast<long long>(kNow - kDay));
  writeFile(own("temporary"), "測試 ㄘㄜˋ-ㄕˋ " + at + "\n");
  writeFile(other("temporary"), "小麥 ㄒㄧㄠˇ-ㄇㄞˋ " + at + "\n");
  auto vocabulary = make(kDeviceID);
  auto result =
      vocabulary->remove(LearnedKind::kTemporary, {"小麥 ㄒㄧㄠˇ-ㄇㄞˋ"});
  EXPECT_EQ(result.status, DeleteStatus::kDeleted);
  EXPECT_EQ(keysOf(vocabulary->rows(LearnedKind::kTemporary, statusTexts())),
            (std::vector<std::string>{"測試 ㄘㄜˋ-ㄕˋ"}));
  EXPECT_NE(readFile(removed()).find("temporary\t小麥 ㄒㄧㄠˇ-ㄇㄞˋ\t"),
            std::string::npos);
}

TEST_F(LearnedVocabularyTest, AddTemporaryPhrasesToPhrases) {
  auto at = std::to_string(static_cast<long long>(kNow - kDay));
  writeFile(own("temporary"), "測試 ㄘㄜˋ-ㄕˋ " + at + "\n" + "小麥 ㄒㄧㄠˇ-ㄇㄞˋ " +
                                  at + "\n" + "留下 ㄌㄧㄡˊ-ㄒㄧㄚˋ " + at + "\n");
  writeFile(folder_ / "data.txt", "# comment\n小麥 ㄒㄧㄠˇ-ㄇㄞˋ\n");
  writeFile(folder_ / "exclude-phrases.txt", "測試 ㄘㄜˋ-ㄕˋ\n");
  auto vocabulary = make(kDeviceID);
  auto result = vocabulary->addToPhrases(
      {"測試 ㄘㄜˋ-ㄕˋ", "小麥 ㄒㄧㄠˇ-ㄇㄞˋ"});
  EXPECT_EQ(result.status, DeleteStatus::kDeleted);
  EXPECT_EQ(result.done, 2u);
  // Taken out of the excluded phrases; one already in data.txt is only
  // forgotten.
  EXPECT_EQ(readFile(folder_ / "data.txt"),
            "# comment\n小麥 ㄒㄧㄠˇ-ㄇㄞˋ\n測試 ㄘㄜˋ-ㄕˋ\n");
  EXPECT_EQ(readFile(folder_ / "exclude-phrases.txt").find("測試"),
            std::string::npos);
  EXPECT_EQ(keysOf(vocabulary->rows(LearnedKind::kTemporary, statusTexts())),
            (std::vector<std::string>{"留下 ㄌㄧㄡˊ-ㄒㄧㄚˋ"}));
  EXPECT_NE(readFile(removed()).find("temporary\t測試 ㄘㄜˋ-ㄕˋ\t"),
            std::string::npos);
  EXPECT_EQ(vocabulary->addToPhrases({}).status,
            DeleteStatus::kNothingSelected);
}

TEST_F(LearnedVocabularyTest, PhraseLearnedMeanwhileIsNotWrittenBack) {
  auto at = std::to_string(static_cast<long long>(kNow - kDay));
  writeFile(own("temporary"), "小麥 ㄒㄧㄠˇ-ㄇㄞˋ " + at + "\n" +
                                  "測試 ㄘㄜˋ-ㄕˋ " + at + "\n" +
                                  "留下 ㄌㄧㄡˊ-ㄒㄧㄚˋ " + at + "\n");
  writeFile(folder_ / "data.txt", "");
  auto vocabulary = make(kDeviceID);
  ASSERT_EQ(vocabulary->rows(LearnedKind::kTemporary, statusTexts()).size(),
            3u);

  // Meanwhile the input method learns 小麥 for good: it takes it out of its
  // own file, recording nothing in removed.txt.
  SmartLearning::TemporaryPhraseStore inputMethod(kDeviceID, kHostName,
                                                  [this] { return now_; });
  inputMethod.setLearningFolder(learning());
  inputMethod.remove("小麥", "ㄒㄧㄠˇ-ㄇㄞˋ");
  ASSERT_TRUE(inputMethod.save());
  ASSERT_EQ(readFile(own("temporary")).find("小麥"), std::string::npos);

  EXPECT_EQ(
      vocabulary->remove(LearnedKind::kTemporary, {"測試 ㄘㄜˋ-ㄕˋ"}).status,
      DeleteStatus::kDeleted);
  EXPECT_EQ(readFile(own("temporary")).find("小麥"), std::string::npos);
  EXPECT_EQ(vocabulary->addToPhrases({"留下 ㄌㄧㄡˊ-ㄒㄧㄚˋ"}).status,
            DeleteStatus::kDeleted);
  EXPECT_EQ(readFile(own("temporary")), "");
  EXPECT_TRUE(
      vocabulary->rows(LearnedKind::kTemporary, statusTexts()).empty());
}

TEST_F(LearnedVocabularyTest, AddToPhrasesWithoutDataFile) {
  auto at = std::to_string(static_cast<long long>(kNow - kDay));
  writeFile(own("temporary"), "測試 ㄘㄜˋ-ㄕˋ " + at + "\n");
  auto vocabulary = make(kDeviceID);
  auto result = vocabulary->addToPhrases({"測試 ㄘㄜˋ-ㄕˋ"});
  EXPECT_EQ(result.status, DeleteStatus::kFileMissing);
  EXPECT_EQ(result.done, 0u);
  EXPECT_FALSE(fs::exists(folder_ / "data.txt"));
  EXPECT_FALSE(fs::exists(removed()));
  EXPECT_EQ(vocabulary->rows(LearnedKind::kTemporary, statusTexts()).size(),
            1u);
}

TEST_F(LearnedVocabularyTest, RemoveStopsAtTheFirstFailure) {
  writeFile(own("english"), "apple\t3\nkiwi\t2\n");
  auto vocabulary = make(kDeviceID);
  // A key with a tab cannot be recorded in removed.txt.
  auto result =
      vocabulary->remove(LearnedKind::kEnglish, {"apple", "bad\tword", "kiwi"});
  EXPECT_EQ(result.status, DeleteStatus::kWriteFailed);
  EXPECT_EQ(result.done, 1u);
  EXPECT_EQ(keysOf(vocabulary->rows(LearnedKind::kEnglish, statusTexts())),
            (std::vector<std::string>{"kiwi"}));
}

TEST_F(LearnedVocabularyTest, PromotedPhraseCountsWhenForgettingFails) {
  auto at = std::to_string(static_cast<long long>(kNow - kDay));
  writeFile(own("temporary"), "測試 ㄘㄜˋ-ㄕˋ " + at + "\n");
  writeFile(folder_ / "data.txt", "");
  // removed.txt cannot be read, so the phrase cannot be forgotten.
  fs::create_directories(removed());
  auto vocabulary = make(kDeviceID);
  auto result = vocabulary->addToPhrases({"測試 ㄘㄜˋ-ㄕˋ"});
  EXPECT_EQ(result.status, DeleteStatus::kWriteFailed);
  // data.txt changed, so the input method is to be told.
  EXPECT_EQ(result.done, 1u);
  EXPECT_EQ(readFile(folder_ / "data.txt"), "測試 ㄘㄜˋ-ㄕˋ\n");
}

// MARK: Without a device ID or a folder

TEST_F(LearnedVocabularyTest, ReadOnlyWithoutDeviceID) {
  auto at = std::to_string(static_cast<long long>(kNow - kDay));
  writeFile(other("english"), "apple\t3\n");
  writeFile(other("temporary"), "小麥 ㄒㄧㄠˇ-ㄇㄞˋ " + at + "\n");
  writeFile(folder_ / "data.txt", "");
  auto vocabulary = make("");
  EXPECT_TRUE(vocabulary->readOnly());
  // The other devices' entries are listed.
  EXPECT_EQ(vocabulary->rows(LearnedKind::kEnglish, statusTexts()).size(), 1u);
  EXPECT_EQ(vocabulary->add(LearnedKind::kEnglish, "okay").status,
            CommitStatus::kReadOnly);
  EXPECT_EQ(vocabulary->add(LearnedKind::kSentences, "下午三點開會").status,
            CommitStatus::kReadOnly);
  EXPECT_EQ(vocabulary->remove(LearnedKind::kEnglish, {"apple"}).status,
            DeleteStatus::kReadOnly);
  EXPECT_EQ(vocabulary->addToPhrases({"小麥 ㄒㄧㄠˇ-ㄇㄞˋ"}).status,
            DeleteStatus::kReadOnly);
  EXPECT_FALSE(fs::exists(removed()));
  EXPECT_EQ(readFile(folder_ / "data.txt"), "");
  EXPECT_EQ(std::distance(fs::directory_iterator(learning() / "english"),
                          fs::directory_iterator()),
            1);
}

TEST_F(LearnedVocabularyTest, MissingDataFolderIsNotCreated) {
  fs::path missing = folder_ / "not-synced";
  LearnedVocabulary vocabulary(missing, kDeviceID, kHostName,
                               [this] { return now_; });
  EXPECT_TRUE(vocabulary.rows(LearnedKind::kEnglish, statusTexts()).empty());
  EXPECT_EQ(vocabulary.add(LearnedKind::kEnglish, "okay").status,
            CommitStatus::kFileMissing);
  EXPECT_EQ(vocabulary.add(LearnedKind::kSentences, "下午三點開會").status,
            CommitStatus::kFileMissing);
  EXPECT_EQ(vocabulary.remove(LearnedKind::kTemporary, {"a b"}).status,
            DeleteStatus::kFileMissing);
  EXPECT_FALSE(fs::exists(missing));
}

// MARK: Opening the window again

TEST(LaunchMessageTest, RoundTrip) {
  std::vector<std::string> arguments{"--data-folder", "/a b/\nc",
                                     "--device-id", "ABCD1234", ""};
  EXPECT_EQ(decodeLaunchMessage(encodeLaunchMessage(arguments)), arguments);
  EXPECT_TRUE(decodeLaunchMessage(encodeLaunchMessage({})).empty());
  // The message of an older window: only brought to the front.
  EXPECT_TRUE(decodeLaunchMessage("show\n").empty());
}

TEST(AdoptLaunchTest, ReadOnlyWindowTakesTheDeviceID) {
  GetEnv env = [](const char*) -> std::optional<std::string> {
    return std::nullopt;
  };
  Settings readOnly;
  readOnly.dataFolder = "/data/old";
  LaunchOptions options;
  options.dataFolder = "/data/new";
  options.deviceID = "ABCD1234";
  auto adopted = adoptLaunch(readOnly, options, env, "host");
  ASSERT_TRUE(adopted.has_value());
  EXPECT_FALSE(adopted->readOnly);
  EXPECT_EQ(adopted->deviceID, "ABCD1234");
  EXPECT_EQ(adopted->dataFolder, fs::path("/data/new"));
  EXPECT_EQ(adopted->ownLearningFilename,
            SmartLearning::learningFilename("ABCD1234", "host"));

  // Without a device ID, or for a window that can already write, nothing
  // changes.
  options.deviceID = "";
  EXPECT_FALSE(adoptLaunch(readOnly, options, env, "host").has_value());
  options.deviceID.reset();
  EXPECT_FALSE(adoptLaunch(readOnly, options, env, "host").has_value());
  Settings writable = *adopted;
  options.deviceID = "FFFF0000";
  EXPECT_FALSE(adoptLaunch(writable, options, env, "host").has_value());
  // The same device in the same folder.
  options.deviceID = "ABCD1234";
  EXPECT_FALSE(adoptLaunch(writable, options, env, "host").has_value());
  options.dataFolder.reset();
  EXPECT_FALSE(adoptLaunch(writable, options, env, "host").has_value());
}

TEST(AdoptLaunchTest, WritableWindowFollowsAChangedDataFolder) {
  GetEnv env = [](const char*) -> std::optional<std::string> {
    return std::nullopt;
  };
  LaunchOptions first;
  first.dataFolder = "/data/old";
  first.deviceID = "ABCD1234";
  Settings writable = resolveSettings(first, env, "host");
  LaunchOptions later = first;
  later.dataFolder = "/data/moved";
  auto adopted = adoptLaunch(writable, later, env, "host");
  ASSERT_TRUE(adopted.has_value());
  EXPECT_EQ(adopted->dataFolder, fs::path("/data/moved"));
  EXPECT_EQ(adopted->deviceID, "ABCD1234");
  EXPECT_FALSE(adopted->readOnly);
  // Another device's folder is not taken.
  later.deviceID = "FFFF0000";
  EXPECT_FALSE(adoptLaunch(writable, later, env, "host").has_value());
}

}  // namespace McBopomofo::VocabularyManager
