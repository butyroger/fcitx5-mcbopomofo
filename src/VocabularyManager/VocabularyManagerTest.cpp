#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>

#include "FileVocabulary.h"
#include "LearningPaths.h"
#include "ManagerSettings.h"

namespace McBopomofo::VocabularyManager {
namespace fs = std::filesystem;

namespace {

void writeFile(const fs::path& path, const std::string& content) {
  std::ofstream out(path, std::ios::binary);
  out << content;
}

std::string readFile(const fs::path& path) {
  std::ifstream in(path, std::ios::binary);
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

class VocabularyManagerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    root_ = fs::temp_directory_path() /
            ("vocabulary-manager-" + std::to_string(stamp) + "-" +
             ::testing::UnitTest::GetInstance()->current_test_info()->name());
    fs::create_directories(root_);
  }
  void TearDown() override {
    std::error_code ec;
    fs::remove_all(root_, ec);
  }

  GetEnv envOf(std::map<std::string, std::string> values) {
    return [values](const char* name) -> std::optional<std::string> {
      auto it = values.find(name);
      if (it == values.end()) {
        return std::nullopt;
      }
      return it->second;
    };
  }

  fs::path root_;
};

}  // namespace

// MARK: Arguments

TEST(VocabularyManagerArgumentsTest, Empty) {
  auto options = parseArguments({});
  EXPECT_FALSE(options.dataFolder.has_value());
  EXPECT_FALSE(options.deviceID.has_value());
  EXPECT_FALSE(options.selfTest);
  EXPECT_TRUE(options.error.empty());
}

TEST(VocabularyManagerArgumentsTest, SeparateAndJoinedValues) {
  auto options = parseArguments(
      {"--data-folder", "/tmp/a b", "--device-id=ABCD1234", "--self-test"});
  EXPECT_EQ(options.dataFolder, "/tmp/a b");
  EXPECT_EQ(options.deviceID, "ABCD1234");
  EXPECT_TRUE(options.selfTest);
  EXPECT_TRUE(options.error.empty());

  options = parseArguments({"--data-folder=/x", "--device-id", ""});
  EXPECT_EQ(options.dataFolder, "/x");
  EXPECT_EQ(options.deviceID, "");
}

TEST(VocabularyManagerArgumentsTest, Errors) {
  EXPECT_EQ(parseArguments({"--data-folder"}).error, "--data-folder");
  EXPECT_EQ(parseArguments({"--bogus"}).error, "--bogus");
  EXPECT_EQ(parseArguments({"stray"}).error, "stray");
}

// MARK: Config file

TEST(VocabularyManagerConfigTest, TopLevelKeys) {
  auto config = parseInputMethodConfig(
      "# comment\n"
      "Layout=Standard\n"
      "UserDataFolder=~/Sync/McBopomofo\n"
      "DeviceID=1A2B3C4D\n"
      "\n"
      "[Group]\n"
      "DeviceID=IGNORED\n");
  EXPECT_EQ(config.userDataFolder, "~/Sync/McBopomofo");
  EXPECT_EQ(config.deviceID, "1A2B3C4D");
}

TEST(VocabularyManagerConfigTest, QuotedAndEmptyValues) {
  auto config = parseInputMethodConfig(
      "UserDataFolder=\"/home/u/My \\\"Sync\\\" \\\\ Folder\"\r\n"
      "DeviceID=\n");
  EXPECT_EQ(config.userDataFolder, "/home/u/My \"Sync\" \\ Folder");
  EXPECT_EQ(config.deviceID, "");
}

TEST(VocabularyManagerConfigTest, MissingKeys) {
  auto config = parseInputMethodConfig("");
  EXPECT_EQ(config.userDataFolder, "");
  EXPECT_EQ(config.deviceID, "");
}

TEST_F(VocabularyManagerTest, Paths) {
  auto env = envOf({{"HOME", "/home/u"}});
  EXPECT_EQ(inputMethodConfigFile(env),
            fs::path("/home/u/.config/fcitx5/conf/mcbopomofo.conf"));
  EXPECT_EQ(defaultUserDataFolder(env),
            fs::path("/home/u/.local/share/fcitx5/mcbopomofo"));

  env = envOf({{"HOME", "/home/u"},
               {"XDG_CONFIG_HOME", "/c"},
               {"XDG_DATA_HOME", "/d"}});
  EXPECT_EQ(inputMethodConfigFile(env),
            fs::path("/c/fcitx5/conf/mcbopomofo.conf"));
  EXPECT_EQ(defaultUserDataFolder(env), fs::path("/d/fcitx5/mcbopomofo"));
}

TEST_F(VocabularyManagerTest, BuiltInDataCandidates) {
  auto env = envOf({{"HOME", "/home/u"}});
  std::vector<fs::path> expected = {
      "/home/u/.local/share/fcitx5/data/mcbopomofo-data.txt",
      "/usr/local/share/fcitx5/data/mcbopomofo-data.txt",
      "/usr/share/fcitx5/data/mcbopomofo-data.txt",
      "/opt/fcitx5/data/mcbopomofo-data.txt",
  };
  EXPECT_EQ(builtInDataCandidates(env, "/opt/fcitx5"), expected);

  env = envOf({{"HOME", "/home/u"},
               {"XDG_DATA_HOME", "/d"},
               {"XDG_DATA_DIRS", "/a::relative:/b/"}});
  expected = {
      "/d/fcitx5/data/mcbopomofo-data.txt",
      "/a/fcitx5/data/mcbopomofo-data.txt",
      "/b/fcitx5/data/mcbopomofo-data.txt",
      "/opt/fcitx5/data/mcbopomofo-data.txt",
  };
  EXPECT_EQ(builtInDataCandidates(env, "/opt/fcitx5"), expected);
}

TEST_F(VocabularyManagerTest, FindBuiltInData) {
  auto env = envOf({{"HOME", (root_ / "home").string()},
                    {"XDG_DATA_DIRS", (root_ / "sys").string()}});
  EXPECT_EQ(findBuiltInData(env, root_ / "pkg"), fs::path());
  fs::create_directories(root_ / "pkg/data");
  writeFile(root_ / "pkg/data/mcbopomofo-data.txt", "x");
  EXPECT_EQ(findBuiltInData(env, root_ / "pkg"),
            root_ / "pkg/data/mcbopomofo-data.txt");
  fs::create_directories(root_ / "sys/fcitx5/data");
  writeFile(root_ / "sys/fcitx5/data/mcbopomofo-data.txt", "x");
  EXPECT_EQ(findBuiltInData(env, root_ / "pkg"),
            root_ / "sys/fcitx5/data/mcbopomofo-data.txt");
}

// MARK: Settings

TEST_F(VocabularyManagerTest, SettingsFromCommandLine) {
  auto env = envOf({{"HOME", (root_ / "home").string()}});
  LaunchOptions options;
  options.dataFolder = (root_ / "data").string();
  options.deviceID = "ABCD1234";
  auto settings = resolveSettings(options, env, "my/host");
  EXPECT_EQ(settings.dataFolder, root_ / "data");
  EXPECT_EQ(settings.deviceID, "ABCD1234");
  EXPECT_FALSE(settings.readOnly);
  // The same file name as the input method's.
  EXPECT_EQ(settings.ownLearningFilename,
            SmartLearning::learningFilename(
                "ABCD1234", SmartLearning::sanitizeDeviceName("my/host")));
  EXPECT_EQ(settings.ownLearningFilename, "ABCD1234-my_host.txt");
}

TEST_F(VocabularyManagerTest, SettingsFromConfigFile) {
  auto home = root_ / "home";
  fs::create_directories(home / ".config/fcitx5/conf");
  fs::create_directories(root_ / "sync/McBopomofo");
  writeFile(home / ".config/fcitx5/conf/mcbopomofo.conf",
            "UserDataFolder=" + (root_ / "sync/McBopomofo").string() +
                "\nDeviceID=0F0F0F0F\n");
  auto env = envOf({{"HOME", home.string()}});
  auto settings = resolveSettings(LaunchOptions{}, env, "");
  EXPECT_EQ(settings.dataFolder, root_ / "sync/McBopomofo");
  EXPECT_EQ(settings.deviceID, "0F0F0F0F");
  EXPECT_FALSE(settings.readOnly);
  EXPECT_EQ(settings.ownLearningFilename, "0F0F0F0F-Linux.txt");
  EXPECT_TRUE(settings.warning.empty());
}

TEST_F(VocabularyManagerTest, SettingsWithoutDeviceIDAreReadOnly) {
  auto home = root_ / "home";
  auto env = envOf({{"HOME", home.string()}});
  // No config file: the default folder, and no device ID.
  auto settings = resolveSettings(LaunchOptions{}, env, "host");
  EXPECT_EQ(settings.dataFolder, home / ".local/share/fcitx5/mcbopomofo");
  EXPECT_TRUE(settings.readOnly);
  EXPECT_EQ(settings.ownLearningFilename, "");

  LaunchOptions options;
  options.dataFolder = (root_ / "data").string();
  options.deviceID = "";
  EXPECT_TRUE(resolveSettings(options, env, "host").readOnly);
}

TEST_F(VocabularyManagerTest, SettingsMissingConfiguredFolderParent) {
  auto home = root_ / "home";
  fs::create_directories(home / ".config/fcitx5/conf");
  writeFile(home / ".config/fcitx5/conf/mcbopomofo.conf",
            "UserDataFolder=" + (root_ / "missing/parent/folder").string() +
                "\nDeviceID=0F0F0F0F\n");
  auto env = envOf({{"HOME", home.string()}});
  auto settings = resolveSettings(LaunchOptions{}, env, "host");
  EXPECT_EQ(settings.dataFolder, home / ".local/share/fcitx5/mcbopomofo");
  EXPECT_FALSE(settings.warning.empty());
}

// MARK: Rows

TEST(VocabularyRowsTest, PhraseRows) {
  auto rows = parseRows(FileKind::kPhrases,
                        "# comment\n"
                        "小麥 ㄒㄧㄠˇ-ㄇㄞˋ\n"
                        "   \n"
                        " #not-comment ㄅ\n"
                        "a\tb\n"
                        "只有一欄\n"
                        "a  b\n"
                        "小麥 ㄒㄧㄠˇ-ㄇㄞˋ\n");
  ASSERT_EQ(rows.size(), 4u);
  EXPECT_EQ(rows[0].columns,
            (std::vector<std::string>{"小麥", "ㄒㄧㄠˇ-ㄇㄞˋ"}));
  EXPECT_EQ(rows[0].key, "小麥 ㄒㄧㄠˇ-ㄇㄞˋ");
  EXPECT_EQ(rows[1].columns,
            (std::vector<std::string>{"#not-comment", "ㄅ"}));
  EXPECT_EQ(rows[1].key, " #not-comment ㄅ");
  // Split once: the rest keeps its space.
  EXPECT_EQ(rows[2].columns, (std::vector<std::string>{"a", " b"}));
  // The same line twice gives two rows with different ids.
  EXPECT_EQ(rows[3].key, rows[0].key);
  EXPECT_NE(rows[3].id, rows[0].id);
}

TEST(VocabularyRowsTest, TemplateRows) {
  auto rows = parseRows(FileKind::kTemplates,
                        "#\tcomment\n"
                        "addr\t1 Sample Road\\nTown\n"
                        "\tempty code\n"
                        "no tab\n"
                        "a\tb\tc\n");
  ASSERT_EQ(rows.size(), 3u);
  EXPECT_EQ(rows[0].columns,
            (std::vector<std::string>{"addr", "1 Sample Road\\nTown"}));
  EXPECT_EQ(rows[1].columns, (std::vector<std::string>{"", "empty code"}));
  EXPECT_EQ(rows[2].columns, (std::vector<std::string>{"a", "b\tc"}));
}

TEST(VocabularyRowsTest, TrimSpaces) {
  EXPECT_EQ(trimSpaces(" \t　小麥  "), "小麥");
  EXPECT_EQ(trimSpaces("\nx\n"), "\nx\n");
  EXPECT_EQ(trimSpaces("   "), "");
}

TEST_F(VocabularyManagerTest, VocabularyFiles) {
  EXPECT_EQ(vocabularyFile(root_, FileKind::kPhrases), root_ / "data.txt");
  EXPECT_EQ(vocabularyFile(root_, FileKind::kExcluded),
            root_ / "exclude-phrases.txt");
  EXPECT_EQ(vocabularyFile(root_, FileKind::kTemplates),
            root_ / "text-templates.txt");
  EXPECT_TRUE(loadRows(FileKind::kPhrases, root_ / "data.txt").empty());
}

// MARK: Commit

TEST_F(VocabularyManagerTest, AddPhrase) {
  auto file = root_ / "data.txt";
  writeFile(file, "# comment\n小麥 ㄒㄧㄠˇ-ㄇㄞˋ");
  auto result = commitEntry(FileKind::kPhrases, file, " 測試 ",
                            " ㄘㄜˋ-ㄕˋ ", std::nullopt);
  EXPECT_EQ(result.status, CommitStatus::kAdded);
  EXPECT_EQ(result.value, "測試");
  EXPECT_EQ(readFile(file), "# comment\n小麥 ㄒㄧㄠˇ-ㄇㄞˋ\n測試 ㄘㄜˋ-ㄕˋ\n");

  result = commitEntry(FileKind::kPhrases, file, "測試", "ㄘㄜˋ-ㄕˋ",
                       std::nullopt);
  EXPECT_EQ(result.status, CommitStatus::kAlreadyListed);
  EXPECT_EQ(result.value, "測試");
}

TEST_F(VocabularyManagerTest, BadReadingsAreNeverWritten) {
  auto file = root_ / "exclude-phrases.txt";
  writeFile(file, "");
  EXPECT_EQ(commitEntry(FileKind::kExcluded, file, "", "ㄅ", std::nullopt)
                .status,
            CommitStatus::kNothingTyped);
  EXPECT_EQ(commitEntry(FileKind::kExcluded, file, "測試", "", std::nullopt)
                .status,
            CommitStatus::kBadReading);
  // A character whose reading was not found.
  EXPECT_EQ(commitEntry(FileKind::kExcluded, file, "測𠀀", "ㄘㄜˋ-？",
                        std::nullopt)
                .status,
            CommitStatus::kBadReading);
  EXPECT_EQ(commitEntry(FileKind::kExcluded, file, "測 試", "ㄘㄜˋ-ㄕˋ",
                        std::nullopt)
                .status,
            CommitStatus::kBadReading);
  EXPECT_EQ(commitEntry(FileKind::kExcluded, file, "測試", "ㄘㄜˋ ㄕˋ",
                        std::nullopt)
                .status,
            CommitStatus::kBadReading);
  EXPECT_EQ(readFile(file), "");
}

TEST_F(VocabularyManagerTest, EditPhrase) {
  auto file = root_ / "data.txt";
  writeFile(file, "甲 ㄐㄧㄚˇ\n乙 ㄧˇ\n甲 ㄐㄧㄚˇ\n");
  auto result = commitEntry(FileKind::kPhrases, file, "丙", "ㄅㄧㄥˇ",
                            std::string("甲 ㄐㄧㄚˇ"));
  EXPECT_EQ(result.status, CommitStatus::kSaved);
  EXPECT_EQ(readFile(file), "丙 ㄅㄧㄥˇ\n乙 ㄧˇ\n甲 ㄐㄧㄚˇ\n");
  // Editing does not look for duplicates.
  result = commitEntry(FileKind::kPhrases, file, "乙", "ㄧˇ",
                       std::string("丙 ㄅㄧㄥˇ"));
  EXPECT_EQ(result.status, CommitStatus::kSaved);
  EXPECT_EQ(readFile(file), "乙 ㄧˇ\n乙 ㄧˇ\n甲 ㄐㄧㄚˇ\n");
}

TEST_F(VocabularyManagerTest, MissingFileIsNotCreated) {
  auto file = root_ / "data.txt";
  EXPECT_EQ(commitEntry(FileKind::kPhrases, file, "測試", "ㄘㄜˋ-ㄕˋ",
                        std::nullopt)
                .status,
            CommitStatus::kFileMissing);
  EXPECT_FALSE(fs::exists(file));
  EXPECT_EQ(deleteEntries(file, {"測試 ㄘㄜˋ-ㄕˋ"}),
            DeleteStatus::kFileMissing);
  EXPECT_FALSE(fs::exists(file));
  auto templates = root_ / "text-templates.txt";
  EXPECT_EQ(commitEntry(FileKind::kTemplates, templates, "addr", "x",
                        std::nullopt)
                .status,
            CommitStatus::kFileMissing);
  EXPECT_FALSE(fs::exists(templates));
}

TEST_F(VocabularyManagerTest, Templates) {
  auto file = root_ / "text-templates.txt";
  writeFile(file, "# templates\n");
  EXPECT_EQ(commitEntry(FileKind::kTemplates, file, "addr", "", std::nullopt)
                .status,
            CommitStatus::kBadTemplate);
  EXPECT_EQ(commitEntry(FileKind::kTemplates, file, "ad\tdr", "x",
                        std::nullopt)
                .status,
            CommitStatus::kBadTemplate);
  EXPECT_EQ(commitEntry(FileKind::kTemplates, file, "addr", "a\tb",
                        std::nullopt)
                .status,
            CommitStatus::kBadTemplate);
  auto result = commitEntry(FileKind::kTemplates, file, "sig",
                            "Regards,\\nSample", std::nullopt);
  EXPECT_EQ(result.status, CommitStatus::kAdded);
  // Templates are not checked for duplicates.
  result = commitEntry(FileKind::kTemplates, file, "sig", "Regards,\\nSample",
                       std::nullopt);
  EXPECT_EQ(result.status, CommitStatus::kAdded);
  EXPECT_EQ(readFile(file),
            "# templates\nsig\tRegards,\\nSample\nsig\tRegards,\\nSample\n");
  result = commitEntry(FileKind::kTemplates, file, "tel", "0000",
                       std::string("sig\tRegards,\\nSample"));
  EXPECT_EQ(result.status, CommitStatus::kSaved);
  EXPECT_EQ(readFile(file),
            "# templates\ntel\t0000\nsig\tRegards,\\nSample\n");
}

// MARK: Delete

TEST_F(VocabularyManagerTest, Delete) {
  auto file = root_ / "data.txt";
  writeFile(file, "# c\n甲 ㄐㄧㄚˇ\n乙 ㄧˇ\n甲 ㄐㄧㄚˇ\n");
  EXPECT_EQ(deleteEntries(file, {}), DeleteStatus::kNothingSelected);
  EXPECT_EQ(deleteEntries(file, {"甲 ㄐㄧㄚˇ", "乙 ㄧˇ"}),
            DeleteStatus::kDeleted);
  EXPECT_EQ(readFile(file), "# c\n甲 ㄐㄧㄚˇ\n");
}

// MARK: Reading

TEST(ReadingFillerTest, MissingData) {
  ReadingFiller filler("/nonexistent/mcbopomofo-data.txt");
  EXPECT_EQ(filler.readingFor("台灣"), "");
}

TEST(ReadingFillerTest, BuiltInData) {
  ReadingFiller filler(MCBOPOMOFO_TEST_FULL_DATA_PATH);
  EXPECT_EQ(filler.readingFor(" 台灣 "), "ㄊㄞˊ-ㄨㄢ");
  EXPECT_EQ(filler.readingFor(""), "");
  EXPECT_EQ(filler.readingFor("   "), "");
  // Nothing found.
  EXPECT_EQ(filler.readingFor("\xF0\xA0\x80\x80"), "");
  // Partly found: shown so the user can fix it, but never written.
  EXPECT_EQ(filler.readingFor("台灣\xF0\xA0\x80\x80"), "ㄊㄞˊ-ㄨㄢ-？");
}

}  // namespace McBopomofo::VocabularyManager
