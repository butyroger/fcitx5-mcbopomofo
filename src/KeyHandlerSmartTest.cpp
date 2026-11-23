// Copyright (c) 2026 and onwards The McBopomofo Authors.
//
// Permission is hereby granted, free of charge, to any person
// obtaining a copy of this software and associated documentation
// files (the "Software"), to deal in the Software without
// restriction, including without limitation the rights to use,
// copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following
// conditions:
//
// The above copyright notice and this permission notice shall be
// included in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
// OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
// HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
// WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
// OTHER DEALINGS IN THE SOFTWARE.

// The smart switching between Chinese and English, with the full language
// model (which knows the readings of letters put as is) and a small English
// word list.

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "EnglishWords.h"
#include "KeyHandler.h"
#include "TemporaryPhrases.h"
#include "UserPhraseFile.h"
#include "gtest/gtest.h"

namespace McBopomofo {
namespace {

using Formosa::Mandarin::BopomofoKeyboardLayout;
using Formosa::Mandarin::BopomofoSyllable;

class NoOpUserPhraseAdder : public UserPhraseAdder {
 public:
  void addUserPhrase(const std::string_view&,
                     const std::string_view&) override {}
  void removeUserPhrase(const std::string_view&,
                        const std::string_view&) override {}
};

class PlainLocalizedStrings : public KeyHandler::LocalizedStrings {
 public:
  std::string cursorIsBetweenSyllables(const std::string&,
                                       const std::string&) override {
    return "";
  }
  std::string syllablesRequired(size_t) override { return ""; }
  std::string syllablesMaximum(size_t) override { return ""; }
  std::string phraseAlreadyExists() override { return ""; }
  std::string pressEnterToAddThePhrase() override { return ""; }
  std::string markedWithSyllablesAndStatus(const std::string&,
                                           const std::string&,
                                           const std::string&) override {
    return "";
  }
  std::string bopomofoFontAnnotationModeTooltip(bool, bool) override {
    return "";
  }
  std::string markingNotAvailableInFontAnnotationMode() override { return ""; }
  std::string pressEscAgainToClearAll() override {
    return "Press Esc again to clear all";
  }
};

// The keys typing the syllables one after another, with a space for the first
// tone.
std::string KeysForSyllables(const BopomofoKeyboardLayout* layout,
                             const std::vector<std::string>& syllables) {
  std::string keys;
  for (const auto& s : syllables) {
    BopomofoSyllable syllable = BopomofoSyllable::FromComposedString(s);
    keys += layout->keySequenceFromSyllable(syllable);
    if (!syllable.hasToneMarker()) {
      keys += " ";
    }
  }
  return keys;
}

class KeyHandlerSmartTest : public ::testing::Test {
 protected:
  void SetUp() override {
    lm_ = std::make_shared<McBopomofoLM>();
    lm_->loadLanguageModel(MCBOPOMOFO_TEST_FULL_DATA_PATH);
    ASSERT_TRUE(lm_->isDataModelLoaded());

    auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    folder_ = std::filesystem::temp_directory_path() /
              ("keyhandler-smart-" + std::to_string(stamp) + "-" +
               ::testing::UnitTest::GetInstance()->current_test_info()->name());
    std::filesystem::create_directories(folder_ / "english");
    overrideStore_ =
        std::make_shared<SmartLearning::UserOverrideStore>("TESTDEV1", "test");
    overrideStore_->setLearningFolder(folder_);

    englishStore_ = std::make_shared<SmartLearning::EnglishWordStore>(
        "TESTDEV1", "test",
        SmartLearning::EnglishLexicon::fromWords(
            {"hello", "help", "helpful", "app", "apple", "you", "ok", "iphone",
             "email", "chat"}),
        [] { return 1000.0; });
    englishStore_->setLearningFolder(folder_);
    lm_->setEnglishCompletionProvider(
        [store = englishStore_](const std::string& word) {
          return store->completions(word);
        });

    keyHandler_ = std::make_unique<KeyHandler>(
        lm_, nullptr, std::make_shared<NoOpUserPhraseAdder>(),
        std::make_unique<PlainLocalizedStrings>(), overrideStore_);
    keyHandler_->setEnglishWordStore(englishStore_);
    keyHandler_->setSmartChineseEnglishSwitchEnabled(true);
    keyHandler_->setAutoSpaceBetweenChineseAndEnglish(true);
    keyHandler_->setFixEnglishWordCase(true);
    keyHandler_->setEscTrimsMistypedText(true);
    useLayout(BopomofoKeyboardLayout::StandardLayout());
    setUpPhraseLearning();
  }

  // The temporary phrases go to the language model, and the phrases learned
  // to data.txt, as the engine does; each write is counted.
  void setUpPhraseLearning() {
    data_ = folder_ / "data.txt";
    exclude_ = folder_ / "exclude-phrases.txt";
    writeFile(data_, "# user phrases\n");
    writeFile(exclude_, "");
    temporaryStore_ = std::make_shared<SmartLearning::TemporaryPhraseStore>(
        "TESTDEV1", "test", [] { return 1700000000.0; });
    temporaryStore_->setLearningFolder(folder_);
    keyHandler_->setTemporaryPhraseStore(temporaryStore_);
    keyHandler_->setUserPhraseLearner(
        [this](const std::vector<std::pair<std::string, std::string>>&
                   phrases) {
          ++learnerCalls_;
          auto learned = SmartLearning::UserPhraseFile::learn(data_, exclude_,
                                                               phrases);
          if (!learned.empty()) {
            ++dataWrites_;
            lm_->loadUserPhrases(data_.c_str(), exclude_.c_str());
          }
          return learned;
        });
    keyHandler_->setOnPhraseLearned([this] {
      ++phraseLearnedCalls_;
      applyTemporaryPhrases();
    });
  }

  void applyTemporaryPhrases() {
    lm_->setTemporaryPhrases(temporaryStore_->phrasesByReading());
  }

  static void writeFile(const std::filesystem::path& path,
                        const std::string& content) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << content;
  }

  static std::string readFile(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
  }

  // Opens the candidates at the end of the composition and picks one.
  void pickCandidate(const std::string& reading, const std::string& value) {
    press(Key::asciiKey(Key::SPACE));
    auto* choosing =
        dynamic_cast<InputStates::ChoosingCandidate*>(state_.get());
    ASSERT_NE(choosing, nullptr);
    keyHandler_->candidateSelected(
        InputStates::ChoosingCandidate::Candidate(reading, value, value),
        choosing->originalCursor,
        [this](std::unique_ptr<InputState> newState) {
          take(std::move(newState));
        });
  }

  // Raising and excluding write the files and read them again.
  void useFileBackedPhrases();

  std::string firstValueOf(const std::string& reading) {
    auto unigrams = lm_->getUnigrams(reading);
    return unigrams.empty() ? "" : unigrams[0].value();
  }

  void TearDown() override {
    std::error_code ec;
    std::filesystem::remove_all(folder_, ec);
  }

  void useLayout(const BopomofoKeyboardLayout* layout) {
    keyHandler_->setKeyboardLayout(layout);
  }

  // Types the keys, starting from the current state, and keeps what is
  // committed.
  void type(const std::string& keys) {
    for (char c : keys) {
      press(Key::asciiKey(c));
    }
  }

  void press(const Key& key) {
    keyHandler_->handle(
        key, state_.get(),
        [this](std::unique_ptr<InputState> newState) {
          take(std::move(newState));
        },
        [] {});
  }

  void take(std::unique_ptr<InputState> newState) {
    if (auto* seq = dynamic_cast<InputStates::StateSequence*>(newState.get())) {
      for (auto& s : seq->states) {
        take(std::move(s));
      }
      return;
    }
    if (auto* committing =
            dynamic_cast<InputStates::Committing*>(newState.get())) {
      committed_ += committing->text;
      state_ = std::make_unique<InputStates::Empty>();
      return;
    }
    if (dynamic_cast<InputStates::EmptyIgnoringPrevious*>(newState.get()) !=
        nullptr) {
      state_ = std::make_unique<InputStates::Empty>();
      return;
    }
    state_ = std::move(newState);
  }

  std::string composingBuffer() {
    auto* inputting = dynamic_cast<InputStates::Inputting*>(state_.get());
    return inputting != nullptr ? inputting->composingBuffer : "";
  }

  std::string tooltip() {
    auto* inputting = dynamic_cast<InputStates::Inputting*>(state_.get());
    return inputting != nullptr ? inputting->tooltip : "";
  }

  bool isEmptyState() {
    return dynamic_cast<InputStates::Empty*>(state_.get()) != nullptr;
  }

  void pressEsc() { press(Key::asciiKey(Key::ESC)); }

  // Presses the key and tells whether it was handled, counting the beeps.
  bool pressHandled(const Key& key) {
    return keyHandler_->handle(
        key, state_.get(),
        [this](std::unique_ptr<InputState> newState) {
          take(std::move(newState));
        },
        [this] { ++errors_; });
  }

  std::vector<std::string> candidateValues() {
    std::vector<std::string> values;
    if (auto* choosing =
            dynamic_cast<InputStates::ChoosingCandidate*>(state_.get())) {
      for (const auto& c : choosing->candidates) {
        values.push_back(c.value);
      }
    }
    return values;
  }

  // Picks a candidate of the candidate window that is shown.
  void selectShownCandidate(const std::string& value) {
    auto* choosing =
        dynamic_cast<InputStates::ChoosingCandidate*>(state_.get());
    ASSERT_NE(choosing, nullptr);
    for (const auto& c : choosing->candidates) {
      if (c.value == value) {
        keyHandler_->candidateSelected(
            c, choosing->originalCursor,
            [this](std::unique_ptr<InputState> newState) {
              take(std::move(newState));
            });
        return;
      }
    }
    FAIL() << value << " is not a candidate";
  }

  void restart() {
    keyHandler_->reset();
    state_ = std::make_unique<InputStates::Empty>();
    committed_.clear();
  }

  std::shared_ptr<McBopomofoLM> lm_;
  std::filesystem::path folder_;
  std::shared_ptr<SmartLearning::UserOverrideStore> overrideStore_;
  std::shared_ptr<SmartLearning::EnglishWordStore> englishStore_;
  std::unique_ptr<KeyHandler> keyHandler_;
  std::filesystem::path data_;
  std::filesystem::path exclude_;
  std::shared_ptr<SmartLearning::TemporaryPhraseStore> temporaryStore_;
  int learnerCalls_ = 0;
  int dataWrites_ = 0;
  int phraseLearnedCalls_ = 0;
  int addHookCalls_ = 0;
  int errors_ = 0;
  std::unique_ptr<InputState> state_ = std::make_unique<InputStates::Empty>();
  std::string committed_;
};

}  // namespace

TEST_F(KeyHandlerSmartTest, ReplacedComponentSwitchesToEnglish) {
  // ㄇㄣ, then ㄣ again.
  type("app");
  EXPECT_EQ(composingBuffer(), "app");
  EXPECT_TRUE(keyHandler_->isSmartEnglishMode());
}

TEST_F(KeyHandlerSmartTest, NonSyllableWithSpaceBecomesEnglishWord) {
  // ㄗㄧㄟ is not a syllable.
  type("you ");
  EXPECT_EQ(composingBuffer(), "you ");
  EXPECT_FALSE(keyHandler_->isSmartEnglishMode());
}

TEST_F(KeyHandlerSmartTest, NonSyllableWithEnterIsCommittedAsEnglish) {
  type("you");
  press(Key::asciiKey(Key::RETURN));
  EXPECT_EQ(committed_, "you");
  EXPECT_EQ(composingBuffer(), "");
}

TEST_F(KeyHandlerSmartTest, UppercaseStartsEnglish) {
  type("ChatGPT");
  EXPECT_EQ(composingBuffer(), "ChatGPT");
  EXPECT_TRUE(keyHandler_->isSmartEnglishMode());
}

TEST_F(KeyHandlerSmartTest, SymbolAfterUnfinishedBopomofo) {
  type("ok?");
  EXPECT_EQ(composingBuffer(), "ok?");
  EXPECT_FALSE(keyHandler_->isSmartEnglishMode());

  // ㄋㄧ, then a key that is not Bopomofo.
  restart();
  type("su?");
  EXPECT_EQ(composingBuffer(), "su?");
  EXPECT_FALSE(keyHandler_->isSmartEnglishMode());
}

TEST_F(KeyHandlerSmartTest, DigitsThenSpaceNoSpace) {
  // 年 is ㄋㄧㄢˊ.
  type("2026 su06");
  EXPECT_EQ(composingBuffer(), "2026年");
  EXPECT_FALSE(keyHandler_->isSmartEnglishMode());
}

TEST_F(KeyHandlerSmartTest, EnglishThenChineseWithoutSpace) {
  useLayout(BopomofoKeyboardLayout::HsuLayout());
  // ㄏㄠˇ right after "hello".
  type("hellohwf");
  EXPECT_EQ(composingBuffer(), "hello 好");
  EXPECT_FALSE(keyHandler_->isSmartEnglishMode());

  // ㄉㄤˇ right after "PDF"; the most likely character of ㄉㄤˇ is picked.
  restart();
  type("PDFdkf");
  EXPECT_EQ(composingBuffer(), "PDF 黨");
  EXPECT_FALSE(keyHandler_->isSmartEnglishMode());

  // Letters that may still be a word are kept: "helpf" for helpful.
  restart();
  type("helpf");
  EXPECT_EQ(composingBuffer(), "helpf");
  EXPECT_TRUE(keyHandler_->isSmartEnglishMode());
}

TEST_F(KeyHandlerSmartTest, AutoSpaceBetweenChineseAndEnglish) {
  type("su3cl3hello");
  EXPECT_EQ(composingBuffer(), "你好 hello");

  restart();
  keyHandler_->setAutoSpaceBetweenChineseAndEnglish(false);
  type("su3cl3hello");
  EXPECT_EQ(composingBuffer(), "你好hello");
}

TEST_F(KeyHandlerSmartTest, CaseFixOnSpaceNotOnEnter) {
  type("iphone ");
  EXPECT_EQ(composingBuffer(), "iPhone ");

  restart();
  type("iphone");
  press(Key::asciiKey(Key::RETURN));
  EXPECT_EQ(committed_, "iphone");

  restart();
  keyHandler_->setFixEnglishWordCase(false);
  type("iphone ");
  EXPECT_EQ(composingBuffer(), "iphone ");
}

TEST_F(KeyHandlerSmartTest, ShortEnglishThatIsBopomofoStaysChinese) {
  useLayout(BopomofoKeyboardLayout::HsuLayout());
  type("a ");
  std::string buffer = composingBuffer();
  ASSERT_FALSE(buffer.empty());
  EXPECT_GE(static_cast<unsigned char>(buffer[0]), 0x80) << buffer;
  EXPECT_EQ(buffer.find('a'), std::string::npos) << buffer;
  EXPECT_FALSE(keyHandler_->isSmartEnglishMode());
}

TEST_F(KeyHandlerSmartTest, ChineseTypingUnchanged) {
  // 我們今天下午去開會; 這個問題需要大家一起討論.
  const std::vector<std::vector<std::string>> sentences = {
      {"ㄨㄛˇ", "ㄇㄣˊ", "ㄐㄧㄣ", "ㄊㄧㄢ", "ㄒㄧㄚˋ", "ㄨˇ", "ㄑㄩˋ", "ㄎㄞ",
       "ㄏㄨㄟˋ"},
      {"ㄓㄜˋ", "ㄍㄜˋ", "ㄨㄣˋ", "ㄊㄧˊ", "ㄒㄩ", "ㄧㄠˋ", "ㄉㄚˋ", "ㄐㄧㄚ",
       "ㄧˋ", "ㄑㄧˇ", "ㄊㄠˇ", "ㄌㄨㄣˋ"},
  };
  for (const auto* layout : {BopomofoKeyboardLayout::StandardLayout(),
                             BopomofoKeyboardLayout::HsuLayout()}) {
    for (const auto& syllables : sentences) {
      std::string keys = KeysForSyllables(layout, syllables);
      std::vector<std::string> buffers;
      for (bool smart : {false, true}) {
        restart();
        useLayout(layout);
        keyHandler_->setSmartChineseEnglishSwitchEnabled(smart);
        type(keys);
        buffers.push_back(composingBuffer());
        EXPECT_FALSE(keyHandler_->isSmartEnglishMode()) << keys;
      }
      EXPECT_EQ(buffers[0], buffers[1]) << keys;
      // All Chinese: no letters, digits or spaces were put as is.
      for (char c : buffers[1]) {
        EXPECT_GE(static_cast<unsigned char>(c), 0x80) << keys << " " << buffers[1];
      }
      EXPECT_GE(buffers[1].size(), syllables.size() * 3) << keys;
    }
  }
}

TEST_F(KeyHandlerSmartTest, LearnsEnglishWordsOnCommit) {
  for (int i = 0; i < 2; i++) {
    restart();
    type("ButyPOS");
    press(Key::asciiKey(Key::RETURN));
    EXPECT_EQ(committed_, "ButyPOS");
  }
  EXPECT_EQ(englishStore_->preferredCase("butypos"), "ButyPOS");
  EXPECT_TRUE(englishStore_->hasUnsavedChanges());
  ASSERT_TRUE(englishStore_->save());
  std::ifstream file(folder_ / "english" / "TESTDEV1-test.txt");
  std::stringstream content;
  content << file.rdbuf();
  EXPECT_EQ(content.str(), "ButyPOS\t2\n");

  restart();
  type("butypos ");
  EXPECT_EQ(composingBuffer(), "ButyPOS ");
}

TEST_F(KeyHandlerSmartTest, ModeChangeCallback) {
  std::vector<bool> changes;
  keyHandler_->setOnSmartEnglishModeChanged(
      [&changes](bool english) { changes.push_back(english); });
  type("app");
  type("le");
  type(" ");
  EXPECT_EQ(changes, (std::vector<bool>{true, false}));
  EXPECT_EQ(composingBuffer(), "apple ");
}

TEST_F(KeyHandlerSmartTest, NoSwitchingWhenTurnedOffOrInPlainBopomofo) {
  keyHandler_->setSmartChineseEnglishSwitchEnabled(false);
  type("app");
  EXPECT_FALSE(keyHandler_->isSmartEnglishMode());
  EXPECT_EQ(composingBuffer(), "ㄇㄣ");

  restart();
  keyHandler_->setSmartChineseEnglishSwitchEnabled(true);
  keyHandler_->setInputMode(InputMode::PlainBopomofo);
  type("app");
  EXPECT_FALSE(keyHandler_->isSmartEnglishMode());
  EXPECT_EQ(composingBuffer(), "ㄇㄣ");
}

TEST_F(KeyHandlerSmartTest, ResetLeavesEnglishMode) {
  type("app");
  ASSERT_TRUE(keyHandler_->isSmartEnglishMode());
  keyHandler_->reset();
  EXPECT_FALSE(keyHandler_->isSmartEnglishMode());
}

TEST_F(KeyHandlerSmartTest, HalfWidthPunctuationAfterDigits) {
  // Moving the cursor leaves the English mode, so the next key is handled as
  // a punctuation key again.
  type("ok300");
  press(Key::namedKey(Key::KeyName::LEFT));
  press(Key::namedKey(Key::KeyName::RIGHT));
  type("!");
  EXPECT_EQ(composingBuffer(), "ok300!");
}

TEST_F(KeyHandlerSmartTest, HalfWidthPunctuationAfterEnglish) {
  type("hello");
  press(Key::namedKey(Key::KeyName::LEFT));
  press(Key::namedKey(Key::KeyName::RIGHT));
  type("!");
  EXPECT_EQ(composingBuffer(), "hello!");

  restart();
  keyHandler_->setHalfWidthPunctuationAfterEnglish(false);
  type("hello");
  press(Key::namedKey(Key::KeyName::LEFT));
  press(Key::namedKey(Key::KeyName::RIGHT));
  type("!");
  EXPECT_EQ(composingBuffer(), "hello！");
}

TEST_F(KeyHandlerSmartTest, FullWidthAfterChinese) {
  type("su3cl3!");
  EXPECT_EQ(composingBuffer(), "你好！");
}

TEST_F(KeyHandlerSmartTest, ControlForcesFullWidth) {
  type("hello");
  press(Key::namedKey(Key::KeyName::LEFT));
  press(Key::namedKey(Key::KeyName::RIGHT));
  press(Key::asciiKey('!', false, true));
  EXPECT_EQ(composingBuffer(), "hello！");
}

TEST_F(KeyHandlerSmartTest, BackspaceRemovesAutoSpace) {
  type("su3cl3iphone");
  ASSERT_EQ(composingBuffer(), "你好 iphone");
  for (int i = 0; i < 6; i++) {
    press(Key::asciiKey(Key::BACKSPACE));
  }
  EXPECT_EQ(composingBuffer(), "你好");
}

TEST_F(KeyHandlerSmartTest, BackspaceKeepsSpaceWhenEnglishRemainsAfterCursor) {
  type("su3cl3hello");
  ASSERT_EQ(composingBuffer(), "你好 hello");
  // Put the cursor after "he" and delete both letters.
  for (int i = 0; i < 3; i++) {
    press(Key::namedKey(Key::KeyName::LEFT));
  }
  press(Key::asciiKey(Key::BACKSPACE));
  press(Key::asciiKey(Key::BACKSPACE));
  EXPECT_EQ(composingBuffer(), "你好 llo");
}

TEST_F(KeyHandlerSmartTest, BackspaceDeletingEnglishKeepsSpaceWhenSettingOff) {
  type("su3cl3hello");
  keyHandler_->setAutoSpaceBetweenChineseAndEnglish(false);
  for (int i = 0; i < 5; i++) {
    press(Key::asciiKey(Key::BACKSPACE));
  }
  EXPECT_EQ(composingBuffer(), "你好 ");
}

TEST_F(KeyHandlerSmartTest, EscTrimsGarbage) {
  // 今天 then garbage letters that cannot be a word.
  type(KeysForSyllables(BopomofoKeyboardLayout::StandardLayout(),
                        {"ㄐㄧㄣ", "ㄊㄧㄢ"}));
  type("Jqvx");
  ASSERT_EQ(composingBuffer(), "今天 Jqvx");
  ASSERT_TRUE(keyHandler_->isSmartEnglishMode());
  pressEsc();
  EXPECT_EQ(composingBuffer(), "今天");
  EXPECT_EQ(tooltip(), "Press Esc again to clear all");
  EXPECT_FALSE(keyHandler_->isSmartEnglishMode());
}

TEST_F(KeyHandlerSmartTest, EscKeepsWordPrefix) {
  type("hellojkq");
  ASSERT_EQ(composingBuffer(), "hellojkq");
  ASSERT_TRUE(keyHandler_->isSmartEnglishMode());
  pressEsc();
  EXPECT_EQ(composingBuffer(), "hello");
  // As with any key that is not a letter, Esc leaves the English mode first.
  EXPECT_FALSE(keyHandler_->isSmartEnglishMode());
}

TEST_F(KeyHandlerSmartTest, EscClearsUnfinishedReadingAndTrailing) {
  // 下午 3 點 followed by an unfinished ㄒㄧ.
  type(KeysForSyllables(BopomofoKeyboardLayout::StandardLayout(),
                        {"ㄒㄧㄚˋ", "ㄨˇ"}));
  type("3 ");
  type(KeysForSyllables(BopomofoKeyboardLayout::StandardLayout(), {"ㄉㄧㄢˇ"}));
  std::string before = composingBuffer();
  type("vu");
  ASSERT_NE(composingBuffer(), before);
  pressEsc();
  EXPECT_EQ(composingBuffer(), before);
}

TEST_F(KeyHandlerSmartTest, EscOnCleanTextOnlyHints) {
  type(KeysForSyllables(BopomofoKeyboardLayout::StandardLayout(),
                        {"ㄋㄧˇ", "ㄏㄠˇ"}));
  std::string text = composingBuffer();
  ASSERT_FALSE(text.empty());
  pressEsc();
  EXPECT_EQ(composingBuffer(), text);
  EXPECT_EQ(tooltip(), "Press Esc again to clear all");
  pressEsc();
  EXPECT_TRUE(isEmptyState());
  EXPECT_EQ(composingBuffer(), "");
}

TEST_F(KeyHandlerSmartTest, EscTwiceAfterTrimmingClearsAll) {
  type("hellojkq");
  pressEsc();
  ASSERT_EQ(composingBuffer(), "hello");
  pressEsc();
  EXPECT_TRUE(isEmptyState());
}

TEST_F(KeyHandlerSmartTest, OtherKeyBreaksDoubleEsc) {
  type("hellojkq");
  pressEsc();
  ASSERT_EQ(composingBuffer(), "hello");
  press(Key::namedKey(Key::KeyName::LEFT));
  press(Key::namedKey(Key::KeyName::RIGHT));
  pressEsc();
  EXPECT_FALSE(isEmptyState());
  EXPECT_EQ(composingBuffer(), "hello");
  EXPECT_EQ(tooltip(), "Press Esc again to clear all");
}

TEST_F(KeyHandlerSmartTest, CandidateWindowKeyBreaksDoubleEsc) {
  type("hellojkq");
  pressEsc();
  keyHandler_->breakEscDoublePress();
  pressEsc();
  EXPECT_FALSE(isEmptyState());
}

TEST_F(KeyHandlerSmartTest, EscOnlyBeforeCursor) {
  type(KeysForSyllables(BopomofoKeyboardLayout::StandardLayout(),
                        {"ㄋㄧˇ", "ㄏㄠˇ"}));
  type("Jqvx");
  std::string full = composingBuffer();
  ASSERT_NE(full.find("Jqvx"), std::string::npos);
  // Move the cursor to just after 你.
  for (int i = 0; i < 4 + 1 + 1; i++) {
    press(Key::namedKey(Key::KeyName::LEFT));
  }
  pressEsc();
  // Nothing before the cursor but Chinese: only the hint, text unchanged.
  EXPECT_EQ(composingBuffer(), full);
  EXPECT_EQ(tooltip(), "Press Esc again to clear all");

  // Garbage in the middle: delete only what is before the cursor.
  restart();
  type("Jqvx ");
  type(KeysForSyllables(BopomofoKeyboardLayout::StandardLayout(),
                        {"ㄋㄧˇ"}));
  ASSERT_EQ(composingBuffer(), "Jqvx 你");
  press(Key::namedKey(Key::KeyName::LEFT));
  pressEsc();
  EXPECT_EQ(composingBuffer(), "你");
}

TEST_F(KeyHandlerSmartTest, OldBehaviorWhenOptionOff) {
  keyHandler_->setEscTrimsMistypedText(false);
  keyHandler_->setEscKeyClearsEntireComposingBuffer(true);
  type("hellojkq");
  pressEsc();
  EXPECT_TRUE(isEmptyState());
}

// MARK: - Learning phrases

// ㄋㄧˇ ㄏㄠˇ, where 你好 wins over 妳好.
constexpr char kNiHaoKeys[] = "su3cl3";
constexpr char kNiHaoReading[] = "ㄋㄧˇ-ㄏㄠˇ";

TEST_F(KeyHandlerSmartTest, PickedPhraseBecomesTemporaryAndWinsNextTime) {
  type(kNiHaoKeys);
  ASSERT_EQ(composingBuffer(), "你好");
  pickCandidate(kNiHaoReading, "妳好");
  EXPECT_EQ(composingBuffer(), "妳好");
  EXPECT_TRUE(temporaryStore_->contains("妳好", kNiHaoReading));
  EXPECT_TRUE(temporaryStore_->hasUnsavedChanges());
  EXPECT_EQ(phraseLearnedCalls_, 1);
  EXPECT_EQ(learnerCalls_, 0);
  // The temporary phrase comes before the built-in ones.
  EXPECT_EQ(firstValueOf(kNiHaoReading), "妳好");

  // Committed in the composition where it was picked: not typed again.
  press(Key::asciiKey(Key::RETURN));
  EXPECT_EQ(committed_, "妳好");
  EXPECT_EQ(learnerCalls_, 0);
  EXPECT_TRUE(temporaryStore_->contains("妳好", kNiHaoReading));
  EXPECT_EQ(readFile(data_), "# user phrases\n");

  // The next composition gives it right away.
  restart();
  type(kNiHaoKeys);
  EXPECT_EQ(composingBuffer(), "妳好");
}

TEST_F(KeyHandlerSmartTest, TemporaryPhraseTypedAgainIsLearnedOnCommit) {
  type(kNiHaoKeys);
  pickCandidate(kNiHaoReading, "妳好");
  press(Key::asciiKey(Key::RETURN));

  restart();
  type(kNiHaoKeys);
  ASSERT_EQ(composingBuffer(), "妳好");
  EXPECT_EQ(learnerCalls_, 0);
  press(Key::asciiKey(Key::RETURN));
  EXPECT_EQ(committed_, "妳好");
  EXPECT_EQ(learnerCalls_, 1);
  EXPECT_EQ(dataWrites_, 1);
  EXPECT_EQ(readFile(data_), "# user phrases\n妳好 ㄋㄧˇ-ㄏㄠˇ\n");
  EXPECT_FALSE(temporaryStore_->contains("妳好", kNiHaoReading));
  EXPECT_EQ(firstValueOf(kNiHaoReading), "妳好");

  // Committed again: no longer temporary, nothing more to write.
  restart();
  type(kNiHaoKeys);
  press(Key::asciiKey(Key::RETURN));
  EXPECT_EQ(committed_, "妳好");
  EXPECT_EQ(learnerCalls_, 1);
}

TEST_F(KeyHandlerSmartTest, PhrasePickedTwiceIsLearned) {
  type(kNiHaoKeys);
  pickCandidate(kNiHaoReading, "妳好");
  ASSERT_TRUE(temporaryStore_->contains("妳好", kNiHaoReading));
  EXPECT_EQ(learnerCalls_, 0);

  // Dropped, not committed, then picked again.
  restart();
  type("cl3");
  type(kNiHaoKeys);
  pickCandidate(kNiHaoReading, "妳好");
  EXPECT_EQ(learnerCalls_, 1);
  EXPECT_EQ(readFile(data_), "# user phrases\n妳好 ㄋㄧˇ-ㄏㄠˇ\n");
  EXPECT_FALSE(temporaryStore_->contains("妳好", kNiHaoReading));
}

TEST_F(KeyHandlerSmartTest, BrokenUpPhraseIsTemporaryThenLearned) {
  // ㄒㄩˇ ㄕˋ gives 許是; the user picks 氏.
  type("vm3g4");
  ASSERT_EQ(composingBuffer(), "許是");
  pickCandidate("ㄕˋ", "氏");
  EXPECT_EQ(composingBuffer(), "許氏");
  EXPECT_TRUE(temporaryStore_->contains("許氏", "ㄒㄩˇ-ㄕˋ"));
  EXPECT_EQ(learnerCalls_, 0);

  // Now 許氏 comes first; picking 氏 there again learns it.
  restart();
  type("vm3g4");
  ASSERT_EQ(composingBuffer(), "許氏");
  pickCandidate("ㄕˋ", "氏");
  EXPECT_EQ(learnerCalls_, 1);
  EXPECT_EQ(readFile(data_), "# user phrases\n許氏 ㄒㄩˇ-ㄕˋ\n");
  EXPECT_FALSE(temporaryStore_->contains("許氏", "ㄒㄩˇ-ㄕˋ"));
}

TEST_F(KeyHandlerSmartTest, CommittingALongSentenceWritesOnce) {
  // Ten temporary phrases from earlier compositions, typed again as one
  // sentence of twenty characters.
  const std::vector<std::pair<std::string, std::string>> phrases = {
      {"我們", "ㄨㄛˇ-ㄇㄣˊ"}, {"今天", "ㄐㄧㄣ-ㄊㄧㄢ"},
      {"下午", "ㄒㄧㄚˋ-ㄨˇ"},  {"去開", "ㄑㄩˋ-ㄎㄞ"},
      {"會這", "ㄏㄨㄟˋ-ㄓㄜˋ"}, {"個問", "ㄍㄜˋ-ㄨㄣˋ"},
      {"題需", "ㄊㄧˊ-ㄒㄩ"},   {"要大", "ㄧㄠˋ-ㄉㄚˋ"},
      {"家一", "ㄐㄧㄚ-ㄧˋ"},   {"起討", "ㄑㄧˇ-ㄊㄠˇ"}};
  std::vector<std::string> syllables;
  std::string sentence;
  std::string expectedData = "# user phrases\n";
  for (const auto& [value, reading] : phrases) {
    temporaryStore_->add(value, reading);
    size_t dash = reading.find('-');
    syllables.push_back(reading.substr(0, dash));
    syllables.push_back(reading.substr(dash + 1));
    sentence += value;
    expectedData += value + " " + reading + "\n";
  }
  applyTemporaryPhrases();
  ASSERT_EQ(syllables.size(), 20u);

  type(KeysForSyllables(BopomofoKeyboardLayout::StandardLayout(), syllables));
  ASSERT_EQ(composingBuffer(), sentence);
  // Typing alone never touches data.txt.
  EXPECT_EQ(learnerCalls_, 0);

  press(Key::asciiKey(Key::RETURN));
  EXPECT_EQ(committed_, sentence);
  EXPECT_EQ(learnerCalls_, 1);
  EXPECT_EQ(dataWrites_, 1);
  EXPECT_EQ(readFile(data_), expectedData);
  for (const auto& [value, reading] : phrases) {
    EXPECT_FALSE(temporaryStore_->contains(value, reading)) << value;
  }
}

TEST_F(KeyHandlerSmartTest, UppercaseLetterCommitDoesNotLearnPhrases) {
  keyHandler_->setSmartChineseEnglishSwitchEnabled(false);
  temporaryStore_->add("妳好", kNiHaoReading);
  applyTemporaryPhrases();
  type(kNiHaoKeys);
  ASSERT_EQ(composingBuffer(), "妳好");
  press(Key::asciiKey('A'));
  EXPECT_EQ(committed_, "妳好A");
  EXPECT_EQ(learnerCalls_, 0);
  EXPECT_TRUE(temporaryStore_->contains("妳好", kNiHaoReading));
}

// MARK: - Raising and excluding candidates

namespace {

// Writes the user phrase files as the engine does and loads them again.
class FileUserPhraseAdder : public UserPhraseAdder {
 public:
  FileUserPhraseAdder(std::shared_ptr<McBopomofoLM> lm,
                      std::filesystem::path data,
                      std::filesystem::path exclude)
      : lm_(std::move(lm)), data_(std::move(data)), exclude_(std::move(exclude)) {}
  void addUserPhrase(const std::string_view& reading,
                     const std::string_view& phrase) override {
    SmartLearning::UserPhraseFile::write(data_, exclude_, std::string(phrase),
                                         std::string(reading));
    lm_->loadUserPhrases(data_.c_str(), exclude_.c_str());
  }
  void removeUserPhrase(const std::string_view& reading,
                        const std::string_view& phrase) override {
    SmartLearning::UserPhraseFile::remove(data_, exclude_, std::string(phrase),
                                          std::string(reading));
    lm_->loadUserPhrases(data_.c_str(), exclude_.c_str());
  }

 private:
  std::shared_ptr<McBopomofoLM> lm_;
  std::filesystem::path data_;
  std::filesystem::path exclude_;
};

}  // namespace

void KeyHandlerSmartTest::useFileBackedPhrases() {
  keyHandler_ = std::make_unique<KeyHandler>(
      lm_, nullptr, std::make_shared<FileUserPhraseAdder>(lm_, data_, exclude_),
      std::make_unique<PlainLocalizedStrings>(), overrideStore_);
  keyHandler_->setOnAddNewPhrase([this](const std::string&) { ++addHookCalls_; });
  useLayout(BopomofoKeyboardLayout::StandardLayout());
}

TEST_F(KeyHandlerSmartTest, BoostingPhraseShowsInCompositionAtOnce) {
  useFileBackedPhrases();
  type(kNiHaoKeys);
  ASSERT_EQ(composingBuffer(), "你好");
  keyHandler_->boostPhrase(kNiHaoReading, "妳好");
  EXPECT_EQ(composingBuffer(), "你好");  // the state is rebuilt by the caller
  take(keyHandler_->buildInputtingState());
  EXPECT_EQ(composingBuffer(), "妳好");
  EXPECT_EQ(addHookCalls_, 1);
}

TEST_F(KeyHandlerSmartTest, ExcludingPhraseShowsInCompositionAtOnce) {
  useFileBackedPhrases();
  type(kNiHaoKeys);
  ASSERT_EQ(composingBuffer(), "你好");
  keyHandler_->boostPhrase(kNiHaoReading, "妳好");
  take(keyHandler_->buildInputtingState());
  ASSERT_EQ(composingBuffer(), "妳好");
  // Excluding the phrase that is now first brings the other one back.
  keyHandler_->excludePhrase(kNiHaoReading, "妳好");
  take(keyHandler_->buildInputtingState());
  EXPECT_EQ(composingBuffer(), "你好");
  EXPECT_EQ(addHookCalls_, 2);
}

TEST_F(KeyHandlerSmartTest, BoostingSingleCharacterShowsInComposition) {
  useFileBackedPhrases();
  type("su3");
  ASSERT_EQ(composingBuffer(), "你");
  keyHandler_->boostPhrase("ㄋㄧˇ", "妳");
  take(keyHandler_->buildInputtingState());
  EXPECT_EQ(composingBuffer(), "妳");
  EXPECT_NE(readFile(data_).find("妳 ㄋㄧˇ"), std::string::npos);

  keyHandler_->excludePhrase("ㄋㄧˇ", "妳");
  take(keyHandler_->buildInputtingState());
  EXPECT_EQ(composingBuffer(), "你");
  EXPECT_NE(readFile(exclude_).find("妳 ㄋㄧˇ"), std::string::npos);
}

TEST_F(KeyHandlerSmartTest, AdjustingKeepsCursorAndOtherPickedCandidates) {
  useFileBackedPhrases();
  type(std::string(kNiHaoKeys) + "su3");
  ASSERT_EQ(composingBuffer(), "你好你");
  // Pick 妳 for the last syllable.
  press(Key::asciiKey(Key::SPACE));
  auto* choosing = dynamic_cast<InputStates::ChoosingCandidate*>(state_.get());
  ASSERT_NE(choosing, nullptr);
  keyHandler_->candidateSelected(
      InputStates::ChoosingCandidate::Candidate("ㄋㄧˇ", "妳", "妳"),
      choosing->originalCursor,
      [this](std::unique_ptr<InputState> s) { take(std::move(s)); });
  ASSERT_EQ(composingBuffer(), "你好妳");

  keyHandler_->boostPhrase(kNiHaoReading, "妳好");
  take(keyHandler_->buildInputtingState());
  EXPECT_EQ(composingBuffer(), "妳好妳");
  auto* inputting = dynamic_cast<InputStates::Inputting*>(state_.get());
  ASSERT_NE(inputting, nullptr);
  EXPECT_EQ(keyHandler_->candidateCursorIndex(), 3u);
}

TEST_F(KeyHandlerSmartTest, TabWithEmptyCompositionGoesToTheApp) {
  EXPECT_FALSE(pressHandled(Key::asciiKey(Key::TAB)));
  EXPECT_FALSE(pressHandled(Key::asciiKey(Key::TAB, /*shiftPressed=*/true)));
  EXPECT_EQ(errors_, 0);
  EXPECT_TRUE(isEmptyState());

  keyHandler_->setSmartChineseEnglishSwitchEnabled(false);
  EXPECT_FALSE(pressHandled(Key::asciiKey(Key::TAB)));
  EXPECT_EQ(errors_, 0);
}

TEST_F(KeyHandlerSmartTest, TabListsEnglishCompletions) {
  type("app");
  ASSERT_TRUE(keyHandler_->isSmartEnglishMode());
  EXPECT_TRUE(pressHandled(Key::asciiKey(Key::TAB)));
  EXPECT_EQ(errors_, 0);
  EXPECT_EQ(candidateValues(), (std::vector<std::string>{"app", "apple"}));

  selectShownCandidate("apple");
  EXPECT_EQ(composingBuffer(), "apple");
  EXPECT_EQ(phraseLearnedCalls_, 0);
  press(Key::asciiKey(Key::RETURN));
  EXPECT_EQ(committed_, "apple");
  // The completion picked was recorded on commit: once more makes it a word
  // typed twice, which comes before the word list.
  englishStore_->record({"apple"});
  EXPECT_EQ(englishStore_->completions("ap")[0], "apple");
}

TEST_F(KeyHandlerSmartTest, TabAfterChineseCompletesOnlyTheEnglishWord) {
  // 好 is ㄏㄠˇ, then a space between Chinese and English.
  type("cl3");
  type("Hel");
  EXPECT_EQ(composingBuffer(), "好 Hel");
  EXPECT_TRUE(pressHandled(Key::asciiKey(Key::TAB)));
  EXPECT_EQ(candidateValues(),
            (std::vector<std::string>{"Hel", "Help", "Hello", "Helpful"}));
  selectShownCandidate("Hello");
  EXPECT_EQ(composingBuffer(), "好 Hello");
}

TEST_F(KeyHandlerSmartTest, TabWithoutCompletionsBeeps) {
  // One letter.
  type("C");
  ASSERT_TRUE(keyHandler_->isSmartEnglishMode());
  EXPECT_TRUE(pressHandled(Key::asciiKey(Key::TAB)));
  EXPECT_EQ(errors_, 1);
  EXPECT_EQ(composingBuffer(), "C");

  // No word starts with the letters.
  restart();
  type("Qzx");
  EXPECT_TRUE(pressHandled(Key::asciiKey(Key::TAB)));
  EXPECT_EQ(errors_, 2);
  EXPECT_EQ(composingBuffer(), "Qzx");
}

TEST_F(KeyHandlerSmartTest, TabAfterUnfinishedReadingCompletesTheKeys) {
  useLayout(BopomofoKeyboardLayout::HsuLayout());
  type("hel");
  EXPECT_EQ(composingBuffer(), "ㄏㄧㄥ");
  EXPECT_FALSE(keyHandler_->isSmartEnglishMode());
  EXPECT_TRUE(pressHandled(Key::asciiKey(Key::TAB)));
  EXPECT_EQ(errors_, 0);
  EXPECT_TRUE(keyHandler_->isSmartEnglishMode());
  auto values = candidateValues();
  ASSERT_FALSE(values.empty());
  EXPECT_EQ(values[0], "hel");
  EXPECT_NE(std::find(values.begin(), values.end(), "hello"), values.end());
  selectShownCandidate("hello");
  EXPECT_EQ(composingBuffer(), "hello");
}

TEST_F(KeyHandlerSmartTest, TabAfterUnfinishedReadingWithoutCompletions) {
  // ㄋㄧ, and no word starts with "su": no beep, the keys become English.
  type("su");
  EXPECT_EQ(composingBuffer(), "ㄋㄧ");
  EXPECT_TRUE(pressHandled(Key::asciiKey(Key::TAB)));
  EXPECT_EQ(errors_, 0);
  EXPECT_EQ(composingBuffer(), "su");
  EXPECT_TRUE(keyHandler_->isSmartEnglishMode());
}

TEST_F(KeyHandlerSmartTest, NoEnglishCompletionWhenSwitchingIsOff) {
  keyHandler_->setSmartChineseEnglishSwitchEnabled(false);
  useLayout(BopomofoKeyboardLayout::HsuLayout());
  type("hel");
  EXPECT_EQ(composingBuffer(), "ㄏㄧㄥ");
  pressHandled(Key::asciiKey(Key::TAB));
  EXPECT_EQ(composingBuffer(), "ㄏㄧㄥ");
  EXPECT_TRUE(candidateValues().empty());
}

// In the standard layout, "+" and ")" after a digit key put the keys as is,
// and "=" after ")" is the full-width punctuation.
TEST_F(KeyHandlerSmartTest, EqualsPutsTheResultOfTheExpression) {
  type("(2+3)=");
  EXPECT_EQ(composingBuffer(), "（2+3)＝5");

  restart();
  type("(2+3)*(1+1)=");
  EXPECT_EQ(composingBuffer(), "（2+3)＊（1+1)＝10");

  restart();
  type("su3cl3(2+3)=");
  EXPECT_EQ(composingBuffer(), "你好（2+3)＝5");
}

TEST_F(KeyHandlerSmartTest, EqualsInTheEnglishModePutsTheResult) {
  // "12" after ")" and "*" is typed in the English mode, and so is "=".
  type("(1+1)*12");
  EXPECT_TRUE(keyHandler_->isSmartEnglishMode());
  type("=");
  EXPECT_EQ(composingBuffer(), "（1+1)＊12=24");
}

TEST_F(KeyHandlerSmartTest, EqualsBeforeTheCursorUsesTheTextBeforeIt) {
  type("(2+3)");
  press(Key::namedKey(Key::KeyName::LEFT));
  type("=");
  EXPECT_EQ(composingBuffer(), "（2+3=5)");
}

TEST_F(KeyHandlerSmartTest, EqualsWithoutAnExpressionIsJustTyped) {
  type("su3cl3=");
  EXPECT_EQ(composingBuffer(), "你好＝");

  restart();
  type("ok12=");
  EXPECT_EQ(composingBuffer(), "ok12=");

  // The result ends the expression, so another "=" adds nothing.
  restart();
  type("(2+3)==");
  EXPECT_EQ(composingBuffer(), "（2+3)＝5=");
}

}  // namespace McBopomofo
