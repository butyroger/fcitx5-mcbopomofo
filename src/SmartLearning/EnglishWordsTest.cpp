#include "EnglishWords.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace McBopomofo::SmartLearning {
namespace fs = std::filesystem;

namespace {

constexpr double kNow = 1700000500;

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

class EnglishWordsTest : public ::testing::Test {
 protected:
  void SetUp() override {
    auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    root_ = fs::temp_directory_path() /
            ("smartlearning-english-" + std::to_string(stamp) + "-" +
             ::testing::UnitTest::GetInstance()->current_test_info()->name());
    learning_ = root_ / "learning";
    english_ = learning_ / "english";
    fs::create_directories(root_);
  }
  void TearDown() override {
    std::error_code ec;
    fs::remove_all(root_, ec);
  }

  std::unique_ptr<EnglishWordStore> makeStore(
      std::vector<std::string> words = {}) {
    return std::make_unique<EnglishWordStore>(
        "TESTDEV1", "test", EnglishLexicon::fromWords(std::move(words)),
        [this] { return now_; });
  }

  fs::path ownFile() const { return english_ / "TESTDEV1-test.txt"; }

  fs::path root_;
  fs::path learning_;
  fs::path english_;
  double now_ = kNow;
};

}  // namespace

TEST(EnglishLexiconTest, LexiconPrefixAndMissingFile) {
  auto lex = EnglishLexicon::fromWords({"Hello", "help", "helpful", "a1b"});
  EXPECT_TRUE(lex->contains("hello"));
  EXPECT_FALSE(lex->contains("a1b"));
  EXPECT_TRUE(lex->hasLongerWordWithPrefix("helpf"));
  EXPECT_TRUE(lex->hasLongerWordWithPrefix("help"));
  EXPECT_FALSE(lex->hasLongerWordWithPrefix("helpful"));
  EXPECT_FALSE(lex->hasLongerWordWithPrefix("helq"));
  EXPECT_FALSE(EnglishLexicon::loadFromFile("/no/such/file")->contains("hello"));
}

TEST_F(EnglishWordsTest, LexiconFileKeepsOnlyAsciiLetterLines) {
  const fs::path list = root_ / "words";
  writeFile(list, "Zebra\ndog\ndog's\nDog\ncaf\xc3\xa9\n\nx-ray\nApple\r\n");
  auto lex = EnglishLexicon::loadFromFile(list);
  EXPECT_EQ(lex->size(), 3u);  // apple, dog, zebra
  EXPECT_TRUE(lex->contains("zebra"));
  EXPECT_TRUE(lex->contains("dog"));
  EXPECT_TRUE(lex->contains("apple"));
  EXPECT_FALSE(lex->contains("dog's"));
  EXPECT_FALSE(lex->contains("x-ray"));
}

TEST(EnglishLexiconTest, SystemWordList) {
  if (!fs::exists(kDefaultEnglishWordList)) {
    GTEST_SKIP() << kDefaultEnglishWordList << " is not installed";
  }
  auto start = std::chrono::steady_clock::now();
  auto lex = EnglishLexicon::loadFromFile(kDefaultEnglishWordList);
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start)
                .count();
  std::cout << "[ word list ] " << lex->size() << " words in " << ms
            << " ms\n";
  EXPECT_GT(lex->size(), 50000u);
  EXPECT_TRUE(lex->contains("hello"));
  EXPECT_TRUE(lex->hasLongerWordWithPrefix("helpf"));

  // Completions are looked up when Tab is pressed, so they must be quick even
  // for a short prefix with many words.
  start = std::chrono::steady_clock::now();
  auto words = lex->wordsWithPrefix("co", 40);
  auto us = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now() - start)
                .count();
  std::cout << "[ word list ] completions of \"co\" in " << us << " us\n";
  EXPECT_EQ(words.size(), 40u);
  EXPECT_LT(us, 50000);
}

TEST_F(EnglishWordsTest, PreferredCaseBuiltinAndLearned) {
  writeFile(ownFile(), "ButyPOS\t2\nOnceWord\t1\n");
  auto store = makeStore();
  store->setLearningFolder(learning_);

  EXPECT_EQ(store->preferredCase("iphone"), "iPhone");
  EXPECT_EQ(store->preferredCase("chatgpt"), "ChatGPT");
  EXPECT_EQ(store->preferredCase("iPhone"), std::nullopt);  // not lowercase
  EXPECT_EQ(store->preferredCase("apple"), std::nullopt);
  EXPECT_EQ(store->preferredCase("butypos"), "ButyPOS");
  EXPECT_EQ(store->preferredCase("onceword"), std::nullopt);  // typed once
  EXPECT_EQ(store->preferredCase("i"), std::nullopt);         // too short
}

TEST_F(EnglishWordsTest, PreferredCaseTieAndLearnedBeforeBuiltin) {
  // Not mixed case: "Zeta" (only the first letter) and "NASA" (all upper).
  writeFile(ownFile(),
            "ZetaQ\t3\nZetaq\t1\nzETAQ\t3\nNASA\t9\nIPhone\t2\nApple\t5\n");
  auto store = makeStore();
  store->setLearningFolder(learning_);
  EXPECT_EQ(store->preferredCase("zetaq"), "ZetaQ");  // tie: smallest key
  EXPECT_EQ(store->preferredCase("iphone"), "IPhone");
  EXPECT_EQ(store->preferredCase("nasa"), std::nullopt);
  EXPECT_EQ(store->preferredCase("apple"), std::nullopt);
}

TEST_F(EnglishWordsTest, IsWordRules) {
  writeFile(ownFile(), "ButyPOS\t2\nOnceWord\t1\n");
  auto store = makeStore({"hello", "helpful"});
  store->setLearningFolder(learning_);

  EXPECT_TRUE(store->isWord("PDF"));      // all uppercase
  EXPECT_TRUE(store->isWord("ChatGPT"));  // two or more trailing uppercase
  EXPECT_TRUE(store->isWord("OK"));
  EXPECT_FALSE(store->isWord("PDFdkf"));
  EXPECT_FALSE(store->isWord("xyzQ"));    // one trailing uppercase
  EXPECT_TRUE(store->isWord("hellO"));    // dictionary, any case
  EXPECT_TRUE(store->isWord("Hello"));
  EXPECT_TRUE(store->isWord("youtube"));  // built-in
  EXPECT_TRUE(store->isWord("butypos"));  // typed twice
  EXPECT_FALSE(store->isWord("onceword"));
  EXPECT_FALSE(store->isWord("h"));
  EXPECT_FALSE(store->isWord("he1lo"));
  EXPECT_FALSE(store->isWord(""));
  EXPECT_FALSE(store->isWord("help"));    // not in this word list
}

TEST_F(EnglishWordsTest, IsWordOrPrefixRules) {
  writeFile(ownFile(), "ButyPOS\t2\nOnceWord\t1\n");
  auto store = makeStore({"hello", "helpful"});
  store->setLearningFolder(learning_);

  EXPECT_TRUE(store->isWordOrPrefix("hello"));
  EXPECT_TRUE(store->isWordOrPrefix("helpf"));
  EXPECT_TRUE(store->isWordOrPrefix("HELPF"));  // all uppercase is a word
  EXPECT_TRUE(store->isWordOrPrefix("Helpf"));
  EXPECT_TRUE(store->isWordOrPrefix("buty"));   // learned twice
  EXPECT_FALSE(store->isWordOrPrefix("oncew"));
  EXPECT_FALSE(store->isWordOrPrefix("helq"));
  EXPECT_FALSE(store->isWordOrPrefix("helpfulx"));
}

TEST_F(EnglishWordsTest, MergesOtherDevicesAndHonorsRemoved) {
  writeFile(english_ / "1778D9F7-Mac Studio.txt", "vercel\t2\nnetlify\t1\n");
  writeFile(english_ / "AAAA0002-laptop.txt", "netlify\t1\n");
  writeFile(learning_ / "removed.txt",
            "english\tvercel\t1700000000\n"
            "sentence\tnetlify\t1700000000\n");
  auto store = makeStore();
  store->setLearningFolder(learning_);
  EXPECT_FALSE(store->isWord("vercel"));
  EXPECT_TRUE(store->isWord("netlify"));  // 1 + 1 over two devices

  // Without the removal, the word from the other device counts.
  writeFile(learning_ / "removed.txt", "english\tvercel\tlater\n");
  auto other = makeStore();
  other->setLearningFolder(learning_);
  EXPECT_TRUE(other->isWord("vercel"));
}

TEST_F(EnglishWordsTest, RemovedFileParsing) {
  writeFile(english_ / "1778D9F7-Mac Studio.txt",
            "alpha\t2\nbravo\t2\ncharlie\t2\ndelta\t2\necho\t2\n");
  writeFile(learning_ / "removed.txt",
            "english\talpha\t1700000000\textra\n"  // four fields
            "english\tbravo\n"                     // two fields
            "english\tcharlie\t0\n"                // time 0: not removed
            "english\tdelta\t\n"                   // empty time
            "english\techo\t12.5\n");              // removed
  auto store = makeStore();
  store->setLearningFolder(learning_);
  EXPECT_TRUE(store->isWord("alpha"));
  EXPECT_TRUE(store->isWord("bravo"));
  EXPECT_TRUE(store->isWord("charlie"));
  EXPECT_TRUE(store->isWord("delta"));
  EXPECT_FALSE(store->isWord("echo"));
}

TEST_F(EnglishWordsTest, NeverOverwritesBeforeLocalLoads) {
  // The user data folder itself is missing, for example a sync folder that
  // has not arrived yet.
  const fs::path dataFolder = root_ / "data";
  const fs::path learning = dataFolder / "learning";
  const fs::path own = learning / "english" / "TESTDEV1-test.txt";
  auto store = makeStore();
  store->setLearningFolder(learning);
  store->record({"newword"});
  EXPECT_TRUE(store->hasUnsavedChanges());
  EXPECT_FALSE(store->save());
  EXPECT_FALSE(fs::exists(dataFolder));
  EXPECT_TRUE(store->hasUnsavedChanges());

  writeFile(own, "oldword\t5\n");
  ASSERT_TRUE(store->save());
  EXPECT_EQ(readFile(own), "newword\t1\noldword\t5\n");
  EXPECT_FALSE(store->hasUnsavedChanges());
}

TEST_F(EnglishWordsTest, MissingLearningFolderIsCreatedOnSave) {
  // root_ is the user data folder; learning/ does not exist yet.
  ASSERT_FALSE(fs::exists(learning_));
  auto store = makeStore();
  store->setLearningFolder(learning_);
  store->record({"zyxwords", "zyxwords"});
  ASSERT_TRUE(store->save());
  EXPECT_EQ(readFile(ownFile()), "zyxwords\t2\n");

  auto other = makeStore();
  other->setLearningFolder(learning_);
  EXPECT_TRUE(other->isWord("zyxwords"));
}

TEST_F(EnglishWordsTest, MissingEnglishFolderIsCreatedOnSave) {
  fs::create_directories(learning_);
  ASSERT_FALSE(fs::exists(english_));
  auto store = makeStore();
  store->setLearningFolder(learning_);
  store->record({"zyxwords", "zyxwords"});
  ASSERT_TRUE(store->save());
  EXPECT_EQ(readFile(ownFile()), "zyxwords\t2\n");

  auto other = makeStore();
  other->setLearningFolder(learning_);
  EXPECT_TRUE(other->isWord("zyxwords"));
}

TEST_F(EnglishWordsTest, EnglishFolderBlockedByFileStaysPending) {
  // A regular file where the english folder should be.
  writeFile(english_, "synthetic");
  auto store = makeStore();
  store->setLearningFolder(learning_);
  store->record({"newword"});
  EXPECT_FALSE(store->save());
  EXPECT_EQ(readFile(english_), "synthetic");
}

TEST_F(EnglishWordsTest, UnreadableOwnFileStaysPending) {
  fs::create_directories(ownFile());  // a folder where the file should be
  auto store = makeStore();
  store->setLearningFolder(learning_);
  store->record({"newword"});
  EXPECT_FALSE(store->save());
  EXPECT_TRUE(fs::is_directory(ownFile()));
}

TEST_F(EnglishWordsTest, EmptyEnglishFolderLoadsEmpty) {
  fs::create_directories(english_);
  auto store = makeStore();
  store->setLearningFolder(learning_);
  store->record({"newword"});
  ASSERT_TRUE(store->save());
  EXPECT_EQ(readFile(ownFile()), "newword\t1\n");
}

TEST_F(EnglishWordsTest, AdoptsRenamedOwnFile) {
  writeFile(english_ / "TESTDEV1-oldname.txt", "oldword\t5\n");
  auto store = makeStore();
  store->setLearningFolder(learning_);
  store->record({"newword"});
  ASSERT_TRUE(store->save());
  EXPECT_FALSE(fs::exists(english_ / "TESTDEV1-oldname.txt"));
  EXPECT_EQ(readFile(ownFile()), "newword\t1\noldword\t5\n");
}

TEST_F(EnglishWordsTest, SaveFormatSortedTabSeparated) {
  fs::create_directories(english_);
  auto store = makeStore();
  store->setLearningFolder(learning_);
  EXPECT_FALSE(store->hasUnsavedChanges());
  store->record({"zeta", "alpha", "alpha", "ab", "Alpha"});
  ASSERT_TRUE(store->save());
  EXPECT_EQ(readFile(ownFile()), "Alpha\t1\nalpha\t2\nzeta\t1\n");
}

TEST_F(EnglishWordsTest, OnlyOwnFileIsWritten) {
  const fs::path macFile = english_ / "1778D9F7-Mac Studio.txt";
  writeFile(macFile, "vercel\t2\n");
  auto store = makeStore();
  store->setLearningFolder(learning_);
  store->record({"vercel"});
  ASSERT_TRUE(store->save());
  EXPECT_EQ(readFile(macFile), "vercel\t2\n");
  EXPECT_EQ(readFile(ownFile()), "vercel\t1\n");
}

TEST_F(EnglishWordsTest, LocalFileReadOnlyOnce) {
  writeFile(ownFile(), "oldword\t5\n");
  auto store = makeStore();
  store->setLearningFolder(learning_);
  EXPECT_TRUE(store->isWord("oldword"));
  writeFile(ownFile(), "");  // memory wins over later changes to the file
  now_ += 120;
  EXPECT_TRUE(store->isWord("oldword"));
  store->record({"newword"});
  ASSERT_TRUE(store->save());
  EXPECT_EQ(readFile(ownFile()), "newword\t1\noldword\t5\n");
}

TEST_F(EnglishWordsTest, ReloadsOtherDevicesAfterSixtySeconds) {
  fs::create_directories(english_);
  auto store = makeStore();
  store->setLearningFolder(learning_);
  EXPECT_FALSE(store->isWord("vercel"));

  writeFile(english_ / "1778D9F7-Mac Studio.txt", "vercel\t2\n");
  now_ = kNow + 59;
  EXPECT_FALSE(store->isWord("vercel"));
  now_ = kNow + 60;
  EXPECT_TRUE(store->isWord("vercel"));

  writeFile(learning_ / "removed.txt", "english\tvercel\t1700000000\n");
  now_ = kNow + 119;
  EXPECT_TRUE(store->isWord("vercel"));
  now_ = kNow + 120;
  EXPECT_FALSE(store->isWord("vercel"));
}

TEST_F(EnglishWordsTest, RecordCountsAtOnce) {
  fs::create_directories(english_);
  auto store = makeStore();
  store->setLearningFolder(learning_);
  store->record({"ButyPOS"});
  EXPECT_FALSE(store->isWord("butypos"));
  store->record({"ButyPOS"});
  EXPECT_TRUE(store->isWord("butypos"));
  EXPECT_EQ(store->preferredCase("butypos"), "ButyPOS");
}

TEST_F(EnglishWordsTest, EmptyOrRelativeFolderIsIgnored) {
  const fs::path oldCwd = fs::current_path();
  fs::current_path(root_);
  {
    auto store = makeStore();
    store->setLearningFolder("");
    store->setLearningFolder("learning");
    store->record({"newword"});
    EXPECT_FALSE(store->save());
  }
  EXPECT_TRUE(fs::is_empty(root_));
  fs::current_path(oldCwd);
}

TEST_F(EnglishWordsTest, SameFolderOnlyReloads) {
  writeFile(ownFile(), "oldword\t5\n");
  auto store = makeStore();
  store->setLearningFolder(learning_);
  store->setLearningFolder(learning_ / "." / "");
  store->record({"newword"});
  ASSERT_TRUE(store->save());
  EXPECT_EQ(readFile(ownFile()), "newword\t1\noldword\t5\n");
}

TEST_F(EnglishWordsTest, SwitchingFoldersSavesOldThenResets) {
  const fs::path second = root_ / "second";
  writeFile(ownFile(), "oldword\t5\n");
  writeFile(second / "english" / "TESTDEV1-test.txt", "otherword\t3\n");
  auto store = makeStore();
  store->setLearningFolder(learning_);
  store->record({"newword"});
  store->setLearningFolder(second);
  EXPECT_EQ(readFile(ownFile()), "newword\t1\noldword\t5\n");
  EXPECT_FALSE(store->isWord("oldword"));
  EXPECT_TRUE(store->isWord("otherword"));
  store->record({"thirdword"});
  ASSERT_TRUE(store->save());
  EXPECT_EQ(readFile(second / "english" / "TESTDEV1-test.txt"),
            "otherword\t3\nthirdword\t1\n");
}

TEST_F(EnglishWordsTest, OwnFileLineParsing) {
  // Lines split on tabs with empty parts dropped; exactly two parts and an
  // integer count.
  writeFile(ownFile(),
            "alpha\t\t2\nbravo\t2\textra\ncharlie\tx\ndelta\t+2\necho\t2\r\n");
  auto store = makeStore();
  store->setLearningFolder(learning_);
  EXPECT_TRUE(store->isWord("alpha"));
  EXPECT_FALSE(store->isWord("bravo"));
  EXPECT_FALSE(store->isWord("charlie"));
  EXPECT_TRUE(store->isWord("delta"));
  EXPECT_FALSE(store->isWord("echo"));
}

TEST(EnglishLexiconTest, WordsWithPrefixShorterFirstThenAlphabetical) {
  auto lex = EnglishLexicon::fromWords(
      {"ass", "assume", "associate", "associated", "associates", "assumption",
       "assay", "bass", "as"});
  EXPECT_EQ(lex->wordsWithPrefix("ass", 40),
            (std::vector<std::string>{"assay", "assume", "associate",
                                      "associated", "associates",
                                      "assumption"}));
  EXPECT_EQ(lex->wordsWithPrefix("ass", 2),
            (std::vector<std::string>{"assay", "assume"}));
  EXPECT_TRUE(lex->wordsWithPrefix("assumption", 40).empty());
  EXPECT_TRUE(lex->wordsWithPrefix("zz", 40).empty());
  EXPECT_TRUE(lex->wordsWithPrefix("ass", 0).empty());
}

TEST_F(EnglishWordsTest, CompletionsFromWordList) {
  auto store = makeStore({"ass", "assume", "associate", "associated",
                          "associates", "assumption", "bass"});
  auto completions = store->completions("ass");
  ASSERT_GE(completions.size(), 2u);
  EXPECT_EQ(completions[0], "assume");
  EXPECT_EQ(completions[1], "associate");
  // The word itself is not a completion.
  EXPECT_EQ(std::count(completions.begin(), completions.end(), "ass"), 0);
  // Fewer than two letters have no completions.
  EXPECT_TRUE(store->completions("a").empty());
  EXPECT_TRUE(store->completions("").empty());
}

TEST_F(EnglishWordsTest, CompletionsCapitalizedLikeTheWord) {
  auto store = makeStore({"hello", "help"});
  EXPECT_EQ(store->completions("Hel"),
            (std::vector<std::string>{"Help", "Hello"}));
}

TEST_F(EnglishWordsTest, CompletionsSkipLettersOnlyForTypedWords) {
  auto store = makeStore({"associated", "aocdx"});
  store->setLearningFolder(learning_);
  // Not typed yet: the word list has no "aocd" prefix of associated.
  EXPECT_EQ(store->completions("aocd"), (std::vector<std::string>{"aocdx"}));
  store->record({"associated"});
  store->record({"associated"});
  auto completions = store->completions("aocd");
  ASSERT_FALSE(completions.empty());
  EXPECT_EQ(completions[0], "associated");
  // The first letter must match.
  EXPECT_TRUE(store->completions("ocd").empty());
}

TEST_F(EnglishWordsTest, CompletionsOrderTypedTwiceDictionaryTypedOnce) {
  auto store = makeStore({"chat", "chatter", "chatroom"});
  store->setLearningFolder(learning_);
  store->record({"ChatGPT", "ChatGPT", "chatbotx"});
  EXPECT_EQ(store->completions("chat"),
            (std::vector<std::string>{"ChatGPT", "chatter", "chatroom",
                                      "chatbotx"}));
  // The most typed first.
  store->record({"chatlogs", "chatlogs", "chatlogs"});
  EXPECT_EQ(store->completions("chat")[0], "chatlogs");
}

TEST_F(EnglishWordsTest, CompletionsAtMostTwenty) {
  std::vector<std::string> words;
  for (char c = 'a'; c <= 'z'; c++) {
    words.push_back(std::string("ab") + c);
  }
  auto store = makeStore(words);
  EXPECT_EQ(store->completions("ab").size(), 20u);
}

TEST_F(EnglishWordsTest, CompletionsLeaveOutRemovedWords) {
  auto store = makeStore();
  writeFile(english_ / "AAAA0002-laptop.txt", "chatlogs\t5\nchatroom\t5\n");
  writeFile(learning_ / "removed.txt", "english\tchatroom\t1700000000\n");
  store->setLearningFolder(learning_);
  EXPECT_EQ(store->completions("chat"),
            (std::vector<std::string>{"chatlogs"}));
}

TEST_F(EnglishWordsTest, CompletionsLeaveOutTheWordItselfInAnyCase) {
  auto store = makeStore({"ass", "assay", "asset"});
  store->setLearningFolder(learning_);
  store->record({"Ass", "Ass", "ASS", "ASS", "aSs"});
  auto completions = store->completions("ass");
  EXPECT_EQ(completions, (std::vector<std::string>{"assay", "asset"}));
  EXPECT_EQ(store->completions("ASS"),
            (std::vector<std::string>{"Assay", "Asset"}));
}

TEST_F(EnglishWordsTest, CompletionsPutWordsWithoutVowelsLast) {
  auto store =
      makeStore({"ass", "assn", "asst", "assay", "asset", "associate"});
  EXPECT_EQ(store->completions("ass"),
            (std::vector<std::string>{"assay", "asset", "associate", "assn",
                                      "asst"}));
  // No vowel at all, and y counts as a vowel.
  auto noVowel = makeStore({"bl", "blvd", "bly", "blew", "blvds"});
  EXPECT_EQ(noVowel->completions("bl"),
            (std::vector<std::string>{"bly", "blew", "blvd", "blvds"}));
  // Short words with only a first vowel, such as and, are not abbreviations.
  auto shortWords = makeStore({"and", "ands", "anew", "ant"});
  EXPECT_EQ(shortWords->completions("an"),
            (std::vector<std::string>{"and", "ant", "anew", "ands"}));
  // Words typed twice still come first and words typed once last.
  store->setLearningFolder(learning_);
  store->record({"assvm", "assvm", "asstx"});
  EXPECT_EQ(store->completions("ass"),
            (std::vector<std::string>{"assvm", "assay", "asset", "associate",
                                      "assn", "asst", "asstx"}));
}

}  // namespace McBopomofo::SmartLearning
