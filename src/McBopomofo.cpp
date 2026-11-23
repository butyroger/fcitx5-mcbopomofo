// Copyright (c) 2022 and onwards The McBopomofo Authors.
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

#include "McBopomofo.h"

#include <fcitx-utils/i18n.h>
#include <fcitx-utils/misc.h>
#include <fcitx/candidatelist.h>
#include <fcitx/event.h>
#include <fcitx/inputcontext.h>
#include <fcitx/userinterfacemanager.h>
#include <fmt/format.h>
#include <notifications_public.h>  // from fcitx-module/notifications

#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "CandidateAdjustment.h"
#include "FloatingComposition.h"
#include "Format.h"
#include "Key.h"
#include "LearningPaths.h"
#include "MixedInput.h"
#include "SymbolTable.h"
#include "TextTemplates.h"
#include "UserDataFolder.h"
#include "UserPhraseFile.h"
#include "Log.h"
#include "UTF8Helper.h"

namespace McBopomofo {

constexpr char kConfigPath[] = "conf/mcbopomofo.conf";

// These two are used to determine whether Shift-[1-9] is pressed.
constexpr int kFcitxRawKeycode_1 = 10;
constexpr int kFcitxRawKeycode_9 = 18;

// For determining whether Shift-Enter is pressed in the candidate panel.
constexpr int kFcitxRawKeycode_Enter = 36;

// Fctix5 notification timeout, in milliseconds.
constexpr int32_t kFcitx5NotificationTimeoutInMs = 1000;

// If a horizontal panel contains a candidate that's longer than this number,
// the panel will be changed to a vertical panel.
constexpr size_t kForceVerticalCandidateThreshold = 8;

static Key MapFcitxKey(const fcitx::Key& key, const fcitx::Key& origKey) {
  bool shiftPressed = key.states() & fcitx::KeyState::Shift;
  bool ctrlPressed = key.states() & fcitx::KeyState::Ctrl;

  // CapsLock state only exists in the original key, so we have to use the
  // origKey variable to check for the state.
  if (origKey.states() & fcitx::KeyState::CapsLock) {
    // When we enter the branch, it means the user want to input Chinese using
    // Bopomofo even when Caps Lock is on, so we swap the uppercase and
    // lowercase letters here.
    fcitx::KeySym sym = key.sym();
    if (sym >= 'A' && sym <= 'Z') {
      return Key::asciiKey(sym + 'a' - 'A',
                           key.states() & fcitx::KeyState::Shift,
                           key.states() & fcitx::KeyState::Ctrl);
    }
    if (sym >= 'a' && sym <= 'z') {
      return Key::asciiKey(sym + 'A' - 'a',
                           key.states() & fcitx::KeyState::Shift,
                           key.states() & fcitx::KeyState::Ctrl);
    }
  }

  if (key.isSimple()) {
    return Key::asciiKey(key.sym(), false);
  }

  if (ctrlPressed && !shiftPressed) {
    switch (key.sym()) {
      case FcitxKey_comma:
        return Key::asciiKey(',', shiftPressed, ctrlPressed);
      case FcitxKey_period:
        return Key::asciiKey('.', shiftPressed, ctrlPressed);
      case FcitxKey_1:
        return Key::asciiKey('!', shiftPressed, ctrlPressed);
      case FcitxKey_slash:
        return Key::asciiKey('/', shiftPressed, ctrlPressed);
      case FcitxKey_semicolon:
        return Key::asciiKey(';', shiftPressed, ctrlPressed);
      case FcitxKey_apostrophe:
        return Key::asciiKey('\'', shiftPressed, ctrlPressed);
      case FcitxKey_backslash:
        return Key::asciiKey('\\', shiftPressed, ctrlPressed);
      default:
        break;
    }
  }

  switch (key.sym()) {
    case FcitxKey_BackSpace:
      return Key::asciiKey(Key::BACKSPACE, shiftPressed, ctrlPressed);
    case FcitxKey_Return:
      return Key::asciiKey(Key::RETURN, shiftPressed, ctrlPressed);
    case FcitxKey_KP_Enter:
      return Key::asciiKey(Key::RETURN, shiftPressed, ctrlPressed, true);
    case FcitxKey_Escape:
      return Key::asciiKey(Key::ESC, shiftPressed, ctrlPressed);
    case FcitxKey_space:
      // This path is taken when Shift is pressed--no longer a "simple" key.
      return Key::asciiKey(Key::SPACE, shiftPressed, ctrlPressed);
    case FcitxKey_Delete:
      return Key::asciiKey(Key::DELETE, shiftPressed, ctrlPressed, true);
    case FcitxKey_KP_Delete:
      return Key::asciiKey(Key::DELETE, shiftPressed, ctrlPressed);
    case FcitxKey_Tab:
      return Key::asciiKey(Key::TAB, shiftPressed, ctrlPressed);
    case FcitxKey_Left:
      return Key::namedKey(Key::KeyName::LEFT, shiftPressed, ctrlPressed);
    case FcitxKey_KP_Left:
      return Key::namedKey(Key::KeyName::LEFT, shiftPressed, ctrlPressed, true);
    case FcitxKey_Right:
      return Key::namedKey(Key::KeyName::RIGHT, shiftPressed, ctrlPressed);
    case FcitxKey_KP_Right:
      return Key::namedKey(Key::KeyName::RIGHT, shiftPressed, ctrlPressed,
                           true);
    case FcitxKey_Home:
      return Key::namedKey(Key::KeyName::HOME, shiftPressed, ctrlPressed);
    case FcitxKey_KP_Home:
      return Key::namedKey(Key::KeyName::HOME, shiftPressed, ctrlPressed, true);
    case FcitxKey_End:
      return Key::namedKey(Key::KeyName::END, shiftPressed, ctrlPressed);
    case FcitxKey_KP_End:
      return Key::namedKey(Key::KeyName::END, shiftPressed, ctrlPressed, true);
    case FcitxKey_Up:
      return Key::namedKey(Key::KeyName::UP, shiftPressed, ctrlPressed, true);
    case FcitxKey_KP_Up:
      return Key::namedKey(Key::KeyName::UP, shiftPressed, ctrlPressed);
    case FcitxKey_Down:
      return Key::namedKey(Key::KeyName::DOWN, shiftPressed, ctrlPressed);
    case FcitxKey_KP_Down:
      return Key::namedKey(Key::KeyName::DOWN, shiftPressed, ctrlPressed, true);
    default:
      break;
  }

  switch (key.sym()) {
    case FcitxKey_KP_0:
      return Key::asciiKey('0', shiftPressed, ctrlPressed, true);
    case FcitxKey_KP_1:
      return Key::asciiKey('1', shiftPressed, ctrlPressed, true);
    case FcitxKey_KP_2:
      return Key::asciiKey('2', shiftPressed, ctrlPressed, true);
    case FcitxKey_KP_3:
      return Key::asciiKey('3', shiftPressed, ctrlPressed, true);
    case FcitxKey_KP_4:
      return Key::asciiKey('4', shiftPressed, ctrlPressed, true);
    case FcitxKey_KP_5:
      return Key::asciiKey('5', shiftPressed, ctrlPressed, true);
    case FcitxKey_KP_6:
      return Key::asciiKey('6', shiftPressed, ctrlPressed, true);
    case FcitxKey_KP_7:
      return Key::asciiKey('7', shiftPressed, ctrlPressed, true);
    case FcitxKey_KP_8:
      return Key::asciiKey('8', shiftPressed, ctrlPressed, true);
    case FcitxKey_KP_9:
      return Key::asciiKey('9', shiftPressed, ctrlPressed, true);
    case FcitxKey_KP_Decimal:
      return Key::asciiKey('.', shiftPressed, ctrlPressed, true);
    case FcitxKey_KP_Add:
      return Key::asciiKey('+', shiftPressed, ctrlPressed, true);
    case FcitxKey_KP_Subtract:
      return Key::asciiKey('-', shiftPressed, ctrlPressed, true);
    case FcitxKey_KP_Multiply:
      return Key::asciiKey('*', shiftPressed, ctrlPressed, true);
    case FcitxKey_KP_Divide:
      return Key::asciiKey('/', shiftPressed, ctrlPressed, true);

    default:
      break;
  }

  return Key{};
}

// The candidate word for the standard candidates.
class McBopomofoCandidateWord : public fcitx::CandidateWord {
 public:
  McBopomofoCandidateWord(fcitx::Text displayText,
                          InputStates::ChoosingCandidate::Candidate candidate,
                          size_t originalCursor,
                          std::shared_ptr<KeyHandler> keyHandler,
                          KeyHandler::StateCallback callback)
      : fcitx::CandidateWord(std::move(displayText)),
        candidate_(std::move(candidate)),
        originalCursor_(originalCursor),
        keyHandler_(std::move(keyHandler)),
        stateCallback_(std::move(callback)) {}

  void select(fcitx::InputContext* /*unused*/) const override {
    keyHandler_->candidateSelected(candidate_, originalCursor_, stateCallback_);
  }

 private:
  InputStates::ChoosingCandidate::Candidate candidate_;
  size_t originalCursor_;
  std::shared_ptr<KeyHandler> keyHandler_;
  KeyHandler::StateCallback stateCallback_;
};

// The candidate word for the associated phrases for McBopomofo mode.
class McBopomofoAssociatedPhraseCandidateWord : public fcitx::CandidateWord {
 public:
  McBopomofoAssociatedPhraseCandidateWord(
      fcitx::Text displayText,
      InputStates::ChoosingCandidate::Candidate associatedPhraseCandidate,
      std::string overrideReading, std::string overrideValue,
      size_t cursorIndex, std::shared_ptr<KeyHandler> keyHandler,
      KeyHandler::StateCallback callback)
      : fcitx::CandidateWord(std::move(displayText)),
        associatedPhraseCandidate_(std::move(associatedPhraseCandidate)),
        keyHandler_(std::move(keyHandler)),
        stateCallback_(std::move(callback)),
        overrideReading_(std::move(overrideReading)),
        overrideValue_(std::move(overrideValue)),
        cursorIndex_(cursorIndex) {}

  void select(fcitx::InputContext* /*unused*/) const override {
    keyHandler_->candidateAssociatedPhraseSelected(
        cursorIndex_, associatedPhraseCandidate_, overrideReading_,
        overrideValue_, stateCallback_);
  }

 private:
  InputStates::ChoosingCandidate::Candidate associatedPhraseCandidate_;
  std::shared_ptr<KeyHandler> keyHandler_;
  KeyHandler::StateCallback stateCallback_;
  std::string overrideReading_;
  std::string overrideValue_;
  size_t cursorIndex_;
};

class McBopomofoDictionaryServiceWord : public fcitx::CandidateWord {
 public:
  McBopomofoDictionaryServiceWord(
      fcitx::Text displayText, size_t index,
      InputStates::SelectingDictionary* currentState,
      std::shared_ptr<KeyHandler> keyHandler,
      KeyHandler::StateCallback callback)
      : fcitx::CandidateWord(std::move(displayText)),
        index_(index),
        currentState_(currentState),
        keyHandler_(std::move(keyHandler)),
        stateCallback_(std::move(callback)) {}

  void select(fcitx::InputContext* /*unused*/) const override {
    keyHandler_->dictionaryServiceSelected(
        currentState_->selectedPhrase, index_, currentState_, stateCallback_);
  }

 private:
  size_t index_;
  InputStates::SelectingDictionary* currentState_;
  std::shared_ptr<KeyHandler> keyHandler_;
  KeyHandler::StateCallback stateCallback_;
};

class McBopomofoFeatureWord : public fcitx::CandidateWord {
 public:
  McBopomofoFeatureWord(fcitx::Text displayText, size_t index,
                        InputStates::SelectingFeature* currentState,
                        std::shared_ptr<KeyHandler> keyHandler,
                        KeyHandler::StateCallback callback)
      : fcitx::CandidateWord(std::move(displayText)),
        index_(index),
        currentState_(currentState),
        keyHandler_(std::move(keyHandler)),
        stateCallback_(std::move(callback)) {}

  void select(fcitx::InputContext* /*unused*/) const override {
    auto nextState = currentState_->nextState(index_);
    stateCallback_(std::move(nextState));
  }

 private:
  size_t index_;
  InputStates::SelectingFeature* currentState_;
  std::shared_ptr<KeyHandler> keyHandler_;
  KeyHandler::StateCallback stateCallback_;
};

class McBopomofoCustomMenuWord : public fcitx::CandidateWord {
 public:
  explicit McBopomofoCustomMenuWord(fcitx::Text displayText, size_t index,
                                    InputStates::CustomMenu* currentState)
      : fcitx::CandidateWord(std::move(displayText)),
        index_(index),
        currentState_(currentState) {}

  void select(fcitx::InputContext* /*unused*/) const override {
    // callback(std::make_unique<InputStates::Committing>(text));
    auto entry = currentState_->entries[index_];
    if (entry.callback) {
      entry.callback();
    }
  }

 private:
  size_t index_;
  InputStates::CustomMenu* currentState_;
};

class McBopomofoDirectInsertWord : public fcitx::CandidateWord {
 public:
  explicit McBopomofoDirectInsertWord(fcitx::Text displayText, std::string text,
                                      KeyHandler::StateCallback callback)
      : fcitx::CandidateWord(std::move(displayText)),
        text(std::move(text)),
        callback(std::move(callback)) {}
  void select(fcitx::InputContext* /*unused*/) const override {
    callback(std::make_unique<InputStates::Committing>(text));
  }

  std::string text;
  KeyHandler::StateCallback callback;
};

class McBopomofoTextOnlyCandidateWord : public fcitx::CandidateWord {
 public:
  explicit McBopomofoTextOnlyCandidateWord(fcitx::Text displayText)
      : fcitx::CandidateWord(std::move(displayText)) {}
  void select(fcitx::InputContext* /*unused*/) const override {}
};

// A row of the symbol table; picking it lists its symbols.
class McBopomofoSymbolRowWord : public fcitx::CandidateWord {
 public:
  McBopomofoSymbolRowWord(fcitx::Text displayText,
                          std::function<void(fcitx::InputContext*)> onSelect)
      : fcitx::CandidateWord(std::move(displayText)),
        onSelect_(std::move(onSelect)) {}
  void select(fcitx::InputContext* context) const override {
    // Showing the symbol table again replaces the candidate list, which
    // destroys this word and |onSelect_|: nothing may follow this call.
    onSelect_(context);
  }

 private:
  std::function<void(fcitx::InputContext*)> onSelect_;
};

class KeyHandlerLocalizedString : public KeyHandler::LocalizedStrings {
 public:
  std::string cursorIsBetweenSyllables(
      const std::string& prevReading, const std::string& nextReading) override {
    return fmt::format(FmtRuntime(_("Cursor is between syllables {0} and {1}")),
                       prevReading, nextReading);
  }

  std::string syllablesRequired(size_t syllables) override {
    return fmt::format(FmtRuntime(_("{0} syllables required")),
                       std::to_string(syllables));
  }

  std::string syllablesMaximum(size_t syllables) override {
    return fmt::format(FmtRuntime(_("{0} syllables maximum")),
                       std::to_string(syllables));
  }

  std::string phraseAlreadyExists() override {
    return _("phrase already exists");
  }

  std::string pressEnterToAddThePhrase() override {
    return _("press Enter to add the phrase");
  }

  std::string markedWithSyllablesAndStatus(const std::string& marked,
                                           const std::string& readingUiText,
                                           const std::string& status) override {
    return fmt::format(FmtRuntime(_("Marked: {0}, syllables: {1}, {2}")),
                       marked, readingUiText, status);
  }

  std::string bopomofoFontAnnotationModeTooltip(bool hasUnicodeVariantSelectors,
                                                bool hasPUABlocks) override {
    if (hasUnicodeVariantSelectors && hasPUABlocks) {
      return _("Bopomofo annotation: variant selectors and PUA blocks in text");
    } else if (hasUnicodeVariantSelectors) {
      return _("Bopomofo annotation: variant selectors in text");
    } else if (hasPUABlocks) {
      return _("Bopomofo annotation: PUA blocks in text");
    } else {
      return _("Bopomofo annotation support on");
    }
  }

  std::string pressEscAgainToClearAll() override {
    return _("Press Esc again to clear all");
  }

  std::string markingNotAvailableInFontAnnotationMode() override {
    return _("cannot add new phrases when Bopomofo annotation is on");
  }

  std::string tabToCompleteSentence(const std::string& rest) override {
    return fmt::format(FmtRuntime(_("Tab: {0}")), rest);
  }

  std::string textTemplatesTitle() override { return _("Text Templates"); }

  std::string quickNumbersTitle() override { return _("Quick Numbers"); }
};

class LanguageModelLoaderLocalizedStrings
    : public LanguageModelLoader::LocalizedStrings {
 public:
  std::string userPhraseFileHeader() override {
    std::stringstream sst;
    // clang-format off
    sst
      << _("# Custom Phrases or Characters.") << "\n"
      << "#\n"
      << _("# See https://github.com/openvanilla/McBopomofo/wiki/使用手冊#手動加詞 for usage.") << "\n"
      << "#\n"
      << _("# Add your phrases and their respective Bopomofo reading below. Use hyphen (\"-\")") << "\n"
      << _("# to connect the Bopomofo syllables.") << "\n"
      << "#\n"
      << "#   小麥注音 ㄒㄧㄠˇ-ㄇㄞˋ-ㄓㄨˋ-ㄧㄣ" << "\n"
      << "#\n"
      << _("# Any line that starts with \"#\" is treated as comment.") << "\n"
      << "\n";
    // clang-format on
    return sst.str();
  }
  std::string excludedPhraseFileHeader() override {
    std::stringstream sst;
    // clang-format off
    sst
      << _("# Custom Excluded Phrases or Characters.") << "\n"
      << "#\n"
      << _("# See https://github.com/openvanilla/McBopomofo/wiki/使用手冊#手動刪詞 for usage.") << "\n"
      << "#\n"
      << _("# For example, the line below will prevent the phrase \"家祠\" from showing up anywhere:") << "\n"
      << "#\n"
      << "#   家祠 ㄐㄧㄚ-ㄘˊ" << "\n"
      << "#\n"
      << _("# Note that you need to use a hyphen (\"-\") between Bopomofo syllables.") << "\n"
      << "#\n"
      << _("# Any line that starts with \"#\" is treated as comment.") << "\n"
      << "\n";
    // clang-format on
    return sst.str();
  }
};

static std::string GetOpenFileWith(const McBopomofoConfig& config) {
  return !config.openUserPhraseFilesWith.value().empty()
             ? config.openUserPhraseFilesWith.value()
             : kDefaultOpenFileWith;
}

McBopomofoEngine::McBopomofoEngine(fcitx::Instance* instance)
    : instance_(instance) {
  // The device ID in the config identifies this device in the learning folder.
  // NOLINTNEXTLINE(clang-analyzer-optin.cplusplus.VirtualCall)
  reloadConfig();
  if (config_.deviceID.value().empty()) {
    config_.deviceID.setValue(SmartLearning::newDeviceID());
    fcitx::safeSaveAsIni(config_, kConfigPath);
  }

  languageModelLoader_ = std::make_shared<LanguageModelLoader>(
      std::make_unique<LanguageModelLoaderLocalizedStrings>());
  userFileIssues_ = languageModelLoader_->getUserFileIssues();
  char hostName[256] = {};
  if (gethostname(hostName, sizeof(hostName) - 1) != 0) {
    hostName[0] = '\0';
  }
  userOverrideStore_ = std::make_shared<SmartLearning::UserOverrideStore>(
      config_.deviceID.value(), hostName);
  auto steadyNow = [] {
    return std::chrono::duration<double>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
  };
  auto wallNow = [] {
    return std::chrono::duration<double>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
  };
  // One reader of removed.txt for all the stores.
  removedEntries_ = std::make_shared<SmartLearning::RemovedEntries>(steadyNow);
  englishWordStore_ = std::make_shared<SmartLearning::EnglishWordStore>(
      config_.deviceID.value(), hostName,
      SmartLearning::EnglishLexicon::loadFromFile(
          SmartLearning::kDefaultEnglishWordList),
      steadyNow, removedEntries_);
  // Temporary phrases and sentences are compared across devices, by the wall
  // clock.
  temporaryPhraseStore_ = std::make_shared<SmartLearning::TemporaryPhraseStore>(
      config_.deviceID.value(), hostName, wallNow, removedEntries_);
  commonSentenceStore_ = std::make_shared<SmartLearning::CommonSentenceStore>(
      config_.deviceID.value(), hostName, wallNow, removedEntries_);
  commonSentenceStore_->setEnabled(config_.commonSentencesEnabled.value());
  textTemplateStore_ =
      std::make_shared<SmartLearning::TextTemplateStore>(steadyNow);
  // The completions listed with Tab for an English word.
  languageModelLoader_->getLM()->setEnglishCompletionProvider(
      [store = englishWordStore_](const std::string& word) {
        return store->completions(word);
      });
  applyUserDataFolder();
  applyLearningFolder();
  keyHandler_ = std::make_shared<KeyHandler>(
      languageModelLoader_->getLM(),
      languageModelLoader_->getVariantAnnotator(), languageModelLoader_,
      std::make_unique<KeyHandlerLocalizedString>(), userOverrideStore_);
  keyHandler_->setOnUserOverrideObserved([this]() {
    userOverrideSavePending_ = true;
    scheduleLearningSave();
  });
  keyHandler_->setTemporaryPhraseStore(temporaryPhraseStore_);
  keyHandler_->setUserPhraseLearner(
      [this](const std::vector<SmartLearning::Phrase>& phrases) {
        return learnUserPhrases(phrases);
      });
  keyHandler_->setOnPhraseLearned([this]() {
    applyTemporaryPhrases();
    if (temporaryPhraseStore_->hasUnsavedChanges()) {
      scheduleLearningSave();
    }
  });
  keyHandler_->setEnglishWordStore(englishWordStore_);
  keyHandler_->setOnEnglishWordsRecorded([this]() {
    if (englishWordStore_->hasUnsavedChanges()) {
      scheduleLearningSave();
    }
  });
  keyHandler_->setCommonSentenceStore(commonSentenceStore_);
  keyHandler_->setTextTemplateStore(textTemplateStore_);
  keyHandler_->setTextTemplateClock([] { return std::time(nullptr); });
  keyHandler_->setOnCommonSentencesRecorded([this]() {
    if (commonSentenceStore_->hasUnsavedChanges()) {
      scheduleLearningSave();
    }
  });
  keyHandler_->setOnAddNewPhrase([this](std::string newPhrase) {
    auto addScriptHookEnabled = config_.addScriptHookEnabled.value();
    if (!addScriptHookEnabled) {
      return;
    }
    auto scriptPath = config_.addScriptHookPath.value();
    if (scriptPath.empty()) {
      scriptPath = kDefaultAddPhraseHookPath;
    }

    auto userDataPath = languageModelLoader_->userDataPath();
    fcitx::startProcess({"/bin/sh", scriptPath, std::move(newPhrase)},
                        userDataPath);
  });

  state_ = std::make_unique<InputStates::Empty>();

  halfWidthPunctuationAction_ = std::make_unique<fcitx::SimpleAction>();
  halfWidthPunctuationAction_->connect<fcitx::SimpleAction::Activated>(
      [this](fcitx::InputContext* context) {
        bool enabled = config_.halfWidthPunctuationEnable.value();
        enabled = !enabled;
        config_.halfWidthPunctuationEnable.setValue(enabled);
        keyHandler_->setHalfWidthPunctuationEnabled(enabled);
        fcitx::safeSaveAsIni(config_, kConfigPath);
        halfWidthPunctuationAction_->setShortText(
            config_.showFullHalfWidthModeIcon.value()
                ? SmartLearning::WidthModeLabel(enabled)
                : (config_.halfWidthPunctuationEnable.value()
                       ? _("Half Width Punctuation")
                       : _("Full Width Punctuation")));
        halfWidthPunctuationAction_->update(context);

        if (notifications()) {
          notifications()->call<fcitx::INotifications::showTip>(
              "mcbopomofo-half-width-punctuation-toggle", _("McBopomofo"),
              "fcitx_mcbopomofo",
              enabled ? _("Half Width Punctuation")
                      : _("Full Width Punctuation"),
              enabled ? _("Now using half width punctuation")
                      : _("Now using full width punctuation"),
              kFcitx5NotificationTimeoutInMs);
        }
      });
  instance_->userInterfaceManager().registerAction(
      "mcbopomofo-half-width-punctuation", halfWidthPunctuationAction_.get());

  layoutStandardAction_ = std::make_unique<fcitx::SimpleAction>();
  layoutStandardAction_->setShortText(_("Bopomofo - Standard Keyboard"));
  layoutStandardAction_->setCheckable(true);
  layoutStandardAction_->connect<fcitx::SimpleAction::Activated>(
      [this](fcitx::InputContext* context) {
        selectQuickLayout(context, BopomofoKeyboardLayout::Standard,
                          _("Bopomofo - Standard Keyboard"));
      });
  instance_->userInterfaceManager().registerAction(
      "mcbopomofo-layout-standard", layoutStandardAction_.get());

  layoutHsuAction_ = std::make_unique<fcitx::SimpleAction>();
  layoutHsuAction_->setShortText(_("Bopomofo - Hsu Keyboard"));
  layoutHsuAction_->setCheckable(true);
  layoutHsuAction_->connect<fcitx::SimpleAction::Activated>(
      [this](fcitx::InputContext* context) {
        selectQuickLayout(context, BopomofoKeyboardLayout::Hsu,
                          _("Bopomofo - Hsu Keyboard"));
      });
  instance_->userInterfaceManager().registerAction("mcbopomofo-layout-hsu",
                                                   layoutHsuAction_.get());

  chineseEnglishModeAction_ = std::make_unique<fcitx::SimpleAction>();
  instance_->userInterfaceManager().registerAction(
      "mcbopomofo-chinese-english-mode", chineseEnglishModeAction_.get());
  keyHandler_->setOnSmartEnglishModeChanged([this](bool /*unused*/) {
    // Only the icon follows the smart English mode, with no reminder.
    updateChineseEnglishIcon(instance_->mostRecentInputContext());
  });

  associatedPhrasesAction_ = std::make_unique<fcitx::SimpleAction>();
  associatedPhrasesAction_->connect<fcitx::SimpleAction::Activated>(
      [this](fcitx::InputContext* context) {
        bool enabled = config_.associatedPhrasesEnabled.value();
        enabled = !enabled;
        config_.associatedPhrasesEnabled.setValue(enabled);
        keyHandler_->setAssociatedPhrasesEnabled(enabled);
        fcitx::safeSaveAsIni(config_, kConfigPath);
        associatedPhrasesAction_->setShortText(
            config_.associatedPhrasesEnabled.value()
                ? _("Associated Phrases - On")
                : _("Associated Phrases - Off"));
        associatedPhrasesAction_->update(context);

        if (notifications()) {
          auto mode = keyHandler_->inputMode();
          notifications()->call<fcitx::INotifications::showTip>(
              "mcbopomofo-associated-phrases-toggle", _("McBopomofo"),
              "fcitx_mcbopomofo",
              enabled ? _("Associated Phrases On")
                      : _("Associated Phrases Off"),
              enabled ? mode == InputMode::McBopomofo
                            ? _("Now you can use Shift + Enter to insert "
                                "associated phrases")
                            : _("Associated Phrases is now enabled.")
                      : _("Associated Phrases is now disabled."),
              kFcitx5NotificationTimeoutInMs);
        }
      });
  instance_->userInterfaceManager().registerAction(
      "mcbopomofo-associated-phrases", associatedPhrasesAction_.get());

  bopomofoFontAnnotationSupportAction_ =
      std::make_unique<fcitx::SimpleAction>();
  bopomofoFontAnnotationSupportAction_->connect<fcitx::SimpleAction::Activated>(
      [this](fcitx::InputContext* context) {
        bool enabled = config_.bopomofoFontAnnotationSupportEnabled.value();
        enabled = !enabled;
        config_.bopomofoFontAnnotationSupportEnabled.setValue(enabled);
        keyHandler_->setBopomofoFontAnnotationSupportEnabled(enabled);
        fcitx::safeSaveAsIni(config_, kConfigPath);
        bopomofoFontAnnotationSupportAction_->setShortText(
            enabled ? _("Bopomofo Font Annotation Support - On")
                    : _("Bopomofo Font Annotation Support - Off"));
        bopomofoFontAnnotationSupportAction_->update(context);

        if (notifications()) {
          notifications()->call<fcitx::INotifications::showTip>(
              "mcbopomofo-bopomofo-font-annotation-support-toggle",
              _("McBopomofo"), "fcitx_mcbopomofo",
              _("Bopomofo Font Annotation Support"),
              enabled ? _("Additional Unicode characters are used to "
                          "support Bopomofo annotations")
                      : _("Bopomofo font annotation support now disabled"),
              kFcitx5NotificationTimeoutInMs);
        }
      });
  instance_->userInterfaceManager().registerAction(
      "mcbopomofo-bopomofo-font-annotation-support-toggle",
      bopomofoFontAnnotationSupportAction_.get());

  editUserPhrasesAction_ = std::make_unique<fcitx::SimpleAction>();
  editUserPhrasesAction_->setShortText(_("Edit User Phrases"));
  editUserPhrasesAction_->connect<fcitx::SimpleAction::Activated>(
      [this](fcitx::InputContext*) {
        fcitx::startProcess({GetOpenFileWith(config_),
                             languageModelLoader_->userPhrasesPath()});
      });
  instance_->userInterfaceManager().registerAction(
      "mcbopomofo-user-phrases-edit", editUserPhrasesAction_.get());

  excludedPhrasesAction_ = std::make_unique<fcitx::SimpleAction>();
  excludedPhrasesAction_->setShortText(_("Edit Excluded Phrases"));
  excludedPhrasesAction_->connect<fcitx::SimpleAction::Activated>(
      [this](fcitx::InputContext*) {
        fcitx::startProcess({GetOpenFileWith(config_),
                             languageModelLoader_->excludedPhrasesPath()});
      });
  instance_->userInterfaceManager().registerAction(
      "mcbopomofo-user-excluded-phrases-edit", excludedPhrasesAction_.get());

  if constexpr (kHasVocabularyManager) {
    manageVocabularyAction_ = std::make_unique<fcitx::SimpleAction>();
    manageVocabularyAction_->setShortText(_("Manage Vocabulary…"));
    manageVocabularyAction_->connect<fcitx::SimpleAction::Activated>(
        [this](fcitx::InputContext*) {
          // The manager writes as this device, in the folder in use.
          fcitx::startProcess(vocabularyManagerCommand(
              kVocabularyManagerPath, languageModelLoader_->userDataPath(),
              config_.deviceID.value()));
        });
    instance_->userInterfaceManager().registerAction(
        "mcbopomofo-manage-vocabulary", manageVocabularyAction_.get());
  }

  floatingCompositionAction_ = std::make_unique<fcitx::SimpleAction>();
  floatingCompositionAction_->setShortText(
      _("Floating Composition Window for This Program"));
  floatingCompositionAction_->setCheckable(true);
  floatingCompositionAction_->connect<fcitx::SimpleAction::Activated>(
      [this](fcitx::InputContext* context) {
        toggleFloatingComposition(context);
      });
  instance_->userInterfaceManager().registerAction(
      "mcbopomofo-floating-composition", floatingCompositionAction_.get());
}

McBopomofoEngine::~McBopomofoEngine() {
  learningSaveTimer_.reset();
  if (userOverrideStore_) {
    userOverrideStore_->save();
  }
  if (englishWordStore_ && englishWordStore_->hasUnsavedChanges()) {
    englishWordStore_->save();
  }
  if (temporaryPhraseStore_ && temporaryPhraseStore_->hasUnsavedChanges()) {
    temporaryPhraseStore_->save();
  }
  if (commonSentenceStore_ && commonSentenceStore_->hasUnsavedChanges()) {
    commonSentenceStore_->save();
  }
}

bool McBopomofoEngine::saveLearning() {
  bool saved = true;
  if (userOverrideSavePending_) {
    if (userOverrideStore_->save()) {
      userOverrideSavePending_ = false;
    } else {
      saved = false;
    }
  }
  if (englishWordStore_->hasUnsavedChanges() && !englishWordStore_->save()) {
    saved = false;
  }
  if (temporaryPhraseStore_->hasUnsavedChanges() &&
      !temporaryPhraseStore_->save()) {
    saved = false;
  }
  if (commonSentenceStore_->hasUnsavedChanges() &&
      !commonSentenceStore_->save()) {
    saved = false;
  }
  return saved;
}

void McBopomofoEngine::applyLearningFolder() {
  if (languageModelLoader_->userDataPath().empty()) {
    return;
  }
  auto learningFolder =
      std::filesystem::path(languageModelLoader_->userDataPath()) / "learning";
  userOverrideStore_->setLearningFolder(learningFolder);
  if (englishWordStore_) {
    englishWordStore_->setLearningFolder(learningFolder);
  }
  if (temporaryPhraseStore_) {
    temporaryPhraseStore_->setLearningFolder(learningFolder);
    applyTemporaryPhrases();
  }
  if (commonSentenceStore_) {
    commonSentenceStore_->setLearningFolder(learningFolder);
  }
  applyTextFiles();
}

void McBopomofoEngine::applyTextFiles() {
  if (languageModelLoader_->userDataPath().empty()) {
    return;
  }
  std::filesystem::path folder(languageModelLoader_->userDataPath());
  if (textTemplateStore_) {
    // The sample is only written to the default folder; a folder chosen by
    // the user may be synced from other computers.
    textTemplateStore_->setFile(
        folder / "text-templates.txt",
        SmartLearning::shouldCreateTemplateFiles(
            folder, languageModelLoader_->defaultUserDataPath())
            ? _("# Text templates. Each line is a code, a tab, and the text. "
                "Write \\n for a line break.\n# Press ` and type the code, "
                "then Enter to insert the text. Press ` again for the symbol "
                "table.\n# The following are examples. Edit or remove them "
                "as you like.\naddr\t100 Main Street, Springfield\ntel\t+1 "
                "555 0100\nem\tname@example.com\nsig\tBest "
                "regards,\\nJohn Doe\n")
            : "");
  }
  symbolTableFile_.setPath(folder / "symbol-table.txt");
  applySymbolTable();
}

void McBopomofoEngine::applySymbolTable() {
  symbolTableFile_.reloadIfChanged(&symbolTable_);
  bool enabled =
      keyHandler_ == nullptr ||
      keyHandler_->inputMode() == McBopomofo::InputMode::McBopomofo;
  languageModelLoader_->getLM()->setSymbolTable(
      enabled ? symbolTable_ : std::vector<SmartLearning::SymbolCategory>());
}

void McBopomofoEngine::applyTemporaryPhrases() {
  bool enabled =
      keyHandler_ == nullptr ||
      keyHandler_->inputMode() == McBopomofo::InputMode::McBopomofo;
  languageModelLoader_->getLM()->setTemporaryPhrases(
      enabled ? temporaryPhraseStore_->phrasesByReading()
              : std::unordered_map<std::string, std::vector<std::string>>());
}

std::vector<SmartLearning::Phrase> McBopomofoEngine::learnUserPhrases(
    const std::vector<SmartLearning::Phrase>& phrases) {
  auto learned = SmartLearning::UserPhraseFile::learn(
      languageModelLoader_->userPhrasesPath(),
      languageModelLoader_->excludedPhrasesPath(), phrases);
  if (learned.empty()) {
    return learned;
  }
  languageModelLoader_->reloadUserPhrases();
  for (const auto& [value, reading] : learned) {
    FCITX_MCBOPOMOFO_INFO() << "Learned phrase: " << value
                            << ", reading: " << reading;
    if (notifications()) {
      std::string message =
          fmt::format(FmtRuntime(_("Learned phrase \"{0}\"")), value);
      notifications()->call<fcitx::INotifications::showTip>(
          "mcbopomofo-learned-phrase", _("McBopomofo"), "fcitx_mcbopomofo",
          message, message, kFcitx5NotificationTimeoutInMs);
    }
  }
  return learned;
}

void McBopomofoEngine::applyUserDataFolder() {
  if (!languageModelLoader_) {
    return;
  }
  std::string warning;
  auto folder = SmartLearning::resolveUserDataFolder(
      config_.userDataFolder.value(),
      languageModelLoader_->defaultUserDataPath(), &warning);
  pendingDataFolderIsFallback_ = !warning.empty();
  if (folder.empty()) {
    return;
  }
  if (folder.string() != languageModelLoader_->userDataPath()) {
    languageModelLoader_->setUserDataFolder(folder);
    userFileIssues_ = languageModelLoader_->getUserFileIssues();
    if (userOverrideStore_) {
      applyLearningFolder();
    }
  }
  // A folder chosen by the user is never populated with template files; tell
  // the user when the real files are not there (yet).
  if (warning.empty() &&
      !SmartLearning::shouldCreateTemplateFiles(
          folder, languageModelLoader_->defaultUserDataPath())) {
    for (const char* name : {"data.txt", "exclude-phrases.txt"}) {
      std::error_code ec;
      if (!std::filesystem::exists(folder / name, ec)) {
        warning = std::string(name) + " was not found in " + folder.string() +
                  "; waiting for it to sync";
        break;
      }
    }
  }
  if (warning.empty()) {
    pendingDataFolderWarning_.clear();
    shownDataFolderWarning_.clear();
  } else {
    pendingDataFolderWarning_ = warning;
  }
}

void McBopomofoEngine::showUserDataFolderWarning() {
  if (pendingDataFolderWarning_.empty() ||
      pendingDataFolderWarning_ == shownDataFolderWarning_) {
    return;
  }
  shownDataFolderWarning_ = pendingDataFolderWarning_;
  std::string body = pendingDataFolderWarning_;
  if (pendingDataFolderIsFallback_) {
    body += "\n";
    body += _("The default folder is used instead.");
  }
  if (notifications()) {
    notifications()->call<fcitx::INotifications::showTip>(
        "mcbopomofo-user-data-folder", _("McBopomofo"), "fcitx_mcbopomofo",
        pendingDataFolderIsFallback_ ? _("User data folder not used")
                                     : _("User data folder is incomplete"),
        body, kFcitx5NotificationTimeoutInMs);
  } else {
    FCITX_MCBOPOMOFO_ERROR() << body;
  }
}

void McBopomofoEngine::scheduleLearningSave() {
  learningSaveRetried_ = false;
  learningSaveTimer_ = instance_->eventLoop().addTimeEvent(
      CLOCK_MONOTONIC, fcitx::now(CLOCK_MONOTONIC) + 2000000, 0,
      [this](fcitx::EventSourceTime* source, uint64_t) {
        // Saving fails while the own file has not been loaded yet (for
        // example it is still being synced); try once more later.
        if (!saveLearning() && !learningSaveRetried_) {
          learningSaveRetried_ = true;
          source->setTime(fcitx::now(CLOCK_MONOTONIC) + 2000000);
          source->setOneShot();
        }
        return true;
      });
}

const fcitx::Configuration* McBopomofoEngine::getConfig() const {
  return &config_;
}

void McBopomofoEngine::setConfig(const fcitx::RawConfig& config) {
  config_.load(config, true);
  fcitx::safeSaveAsIni(config_, kConfigPath);
  applyUserDataFolder();
  applySmartOptions();
}

void McBopomofoEngine::reloadConfig() {
  fcitx::readAsIni(config_, kConfigPath);
  applyUserDataFolder();
  applySmartOptions();
  // The vocabulary manager asks for this after writing the learned data,
  // the user phrases, the excluded phrases or the text templates.
  recheckLearningFiles();
  // The constructor reads the config before the language model exists.
  if (languageModelLoader_ && languageModelLoader_->reloadUserModelsIfNeeded()) {
    userFileIssues_ = languageModelLoader_->getUserFileIssues();
  }
}

void McBopomofoEngine::recheckLearningFiles() {
  // The constructor reads the config before the stores exist.
  if (!removedEntries_) {
    return;
  }
  removedEntries_->reloadIfChanged();
  userOverrideStore_->reloadIfChanged();
  englishWordStore_->reload();
  commonSentenceStore_->reload();
  temporaryPhraseStore_->reload();
  applyTemporaryPhrases();
}

void McBopomofoEngine::applySmartOptions() {
  if (commonSentenceStore_) {
    commonSentenceStore_->setEnabled(config_.commonSentencesEnabled.value());
  }
  if (!keyHandler_) {
    return;
  }
  keyHandler_->setEscTrimsMistypedText(config_.escTrimsMistypedText.value());
  keyHandler_->setSmartChineseEnglishSwitchEnabled(
      config_.smartChineseEnglishSwitchEnabled.value());
  keyHandler_->setAutoSpaceBetweenChineseAndEnglish(
      config_.autoSpaceBetweenChineseAndEnglish.value());
  keyHandler_->setHalfWidthPunctuationAfterEnglish(
      config_.halfWidthPunctuationAfterEnglish.value());
  keyHandler_->setFixEnglishWordCase(config_.fixEnglishWordCase.value());
  keyHandler_->setKeyboardLayout(
      layoutForConfig(config_.bopomofoKeyboardLayout.value()));
  if (auto* context = instance_->mostRecentInputContext()) {
    refreshModeIcons(context);
    // The list may have been edited in the config tool.
    if (floatingCompositionAction_) {
      floatingCompositionAction_->setChecked(
          SmartLearning::UsesFloatingComposition(
              context->program(), config_.floatingCompositionPrograms.value()));
      floatingCompositionAction_->update(context);
    }
  }
}

bool McBopomofoEngine::useClientPreedit(fcitx::InputContext* context) const {
  return SmartLearning::UseClientPreedit(
      context->capabilityFlags().test(fcitx::CapabilityFlag::Preedit),
      context->program(), config_.floatingCompositionPrograms.value());
}

void McBopomofoEngine::refreshFloatingCompositionAction(
    fcitx::InputContext* context) {
  auto& statusArea = context->statusArea();
  // Without a program name there is nothing to remember it by.
  if (context->program().empty()) {
    statusArea.removeAction(floatingCompositionAction_.get());
    return;
  }
  floatingCompositionAction_->setChecked(SmartLearning::UsesFloatingComposition(
      context->program(), config_.floatingCompositionPrograms.value()));
  floatingCompositionAction_->update(context);
  statusArea.addAction(fcitx::StatusGroup::InputMethod,
                       floatingCompositionAction_.get());
}

void McBopomofoEngine::toggleFloatingComposition(
    fcitx::InputContext* context) {
  if (context == nullptr || context->program().empty()) {
    return;
  }
  // Whatever is being composed is committed first, so that no text is left
  // shown where it no longer belongs.
  keyHandler_->reset();
  enterNewState(context, std::make_unique<InputStates::Empty>());

  const std::string program = context->program();
  auto toggled = SmartLearning::ToggleFloatingComposition(
      config_.floatingCompositionPrograms.value(), program);
  config_.floatingCompositionPrograms.setValue(std::move(toggled.programs));
  fcitx::safeSaveAsIni(config_, kConfigPath);

  if (notifications()) {
    std::string message = fmt::format(
        FmtRuntime(toggled.enabled
                       ? _("Floating composition window on for {0}")
                       : _("Floating composition window off for {0}")),
        program);
    notifications()->call<fcitx::INotifications::showTip>(
        "mcbopomofo-floating-composition-toggle", _("McBopomofo"),
        "fcitx_mcbopomofo", message, message, kFcitx5NotificationTimeoutInMs);
  }
  refreshFloatingCompositionAction(context);
}

void McBopomofoEngine::refreshLayoutActions(fcitx::InputContext* context) {
  auto configured = config_.bopomofoKeyboardLayout.value();
  layoutStandardAction_->setChecked(
      isQuickLayoutChecked(configured, BopomofoKeyboardLayout::Standard));
  layoutHsuAction_->setChecked(
      isQuickLayoutChecked(configured, BopomofoKeyboardLayout::Hsu));
  if (context != nullptr) {
    layoutStandardAction_->update(context);
    layoutHsuAction_->update(context);
  }
}

void McBopomofoEngine::selectQuickLayout(fcitx::InputContext* context,
                                         BopomofoKeyboardLayout layout,
                                         const char* title) {
  config_.bopomofoKeyboardLayout.setValue(layout);
  fcitx::safeSaveAsIni(config_, kConfigPath);
  keyHandler_->setKeyboardLayout(layoutForConfig(layout));
  // Whatever was being composed is dropped, not committed: switching the
  // keyboard must not put half-typed text into the application. Entering
  // EmptyIgnoringPrevious first discards the old state without side effects;
  // Empty follows so the rest of the engine only ever sees Empty.
  keyHandler_->reset();
  if (context != nullptr) {
    auto seq = std::make_unique<InputStates::StateSequence>();
    seq->push_back(std::make_unique<InputStates::EmptyIgnoringPrevious>());
    seq->push_back(std::make_unique<InputStates::Empty>());
    handleStateOrSequence(context, std::move(seq));
  }
  if (notifications()) {
    notifications()->call<fcitx::INotifications::showTip>(
        "mcbopomofo-layout-switch", _("McBopomofo"), "fcitx_mcbopomofo", title,
        title, kFcitx5NotificationTimeoutInMs);
  }
  refreshLayoutActions(context);
}

void McBopomofoEngine::updateChineseEnglishIcon(
    fcitx::InputContext* context) {
  chineseEnglishModeAction_->setShortText(SmartLearning::LanguageModeLabel(
      capsLockOn_, keyHandler_->isSmartEnglishMode()));
  if (context != nullptr) {
    chineseEnglishModeAction_->update(context);
  }
}

void McBopomofoEngine::refreshModeIcons(fcitx::InputContext* context) {
  refreshLayoutActions(context);
  auto& statusArea = context->statusArea();
  if (config_.showChineseEnglishModeIcon.value()) {
    // Also shown in Plain mode, where it reflects Caps Lock only.
    updateChineseEnglishIcon(context);
    statusArea.addAction(fcitx::StatusGroup::InputMethod,
                         chineseEnglishModeAction_.get());
  } else {
    statusArea.removeAction(chineseEnglishModeAction_.get());
  }

  bool halfWidth = config_.halfWidthPunctuationEnable.value();
  halfWidthPunctuationAction_->setShortText(
      config_.showFullHalfWidthModeIcon.value()
          ? SmartLearning::WidthModeLabel(halfWidth)
          : (halfWidth ? _("Half width Punctuation")
                       : _("Full Width Punctuation")));
  halfWidthPunctuationAction_->update(context);
}

void McBopomofoEngine::activate(const fcitx::InputMethodEntry& entry,
                                fcitx::InputContextEvent& event) {
  McBopomofo::InputMode mode = entry.uniqueName() == "mcbopomofo-plain"
                                   ? McBopomofo::InputMode::PlainBopomofo
                                   : McBopomofo::InputMode::McBopomofo;

  if (mode != keyHandler_->inputMode()) {
    languageModelLoader_->loadModelForMode(mode);
  }

  // fcitx does not expose the Caps Lock state here, so the first key event
  // adopts it as the baseline without a reminder.
  capsLockKnown_ = false;

  chttrans();

  auto* inputContext = event.inputContext();
  // The two keyboard items come first in the menu.
  inputContext->statusArea().addAction(fcitx::StatusGroup::InputMethod,
                                       layoutStandardAction_.get());
  inputContext->statusArea().addAction(fcitx::StatusGroup::InputMethod,
                                       layoutHsuAction_.get());
  if (auto* action =
          instance_->userInterfaceManager().lookupAction("chttrans")) {
    inputContext->statusArea().addAction(fcitx::StatusGroup::InputMethod,
                                         action);
  }

  inputContext->statusArea().addAction(fcitx::StatusGroup::InputMethod,
                                       halfWidthPunctuationAction_.get());
  refreshModeIcons(inputContext);

  associatedPhrasesAction_->setShortText(
      config_.associatedPhrasesEnabled.value() ? _("Associated Phrases - On")
                                               : _("Associated Phrases - Off"));
  associatedPhrasesAction_->update(inputContext);
  inputContext->statusArea().addAction(fcitx::StatusGroup::InputMethod,
                                       associatedPhrasesAction_.get());

  // Even if the toggle is disabled by
  // showBopomofoFontAnnotationSupportInMenu, we'll still want to show the
  // item if bopomofoFontAnnotationSupportEnabled is on, so that the user has
  // a chance to disable it. We never show this menu item in Plain Bopomofo,
  // since we don't support annotation there.
  if (mode == McBopomofo::InputMode::McBopomofo &&
      (config_.showBopomofoFontAnnotationSupportInMenu.value() ||
       config_.bopomofoFontAnnotationSupportEnabled.value())) {
    bopomofoFontAnnotationSupportAction_->setShortText(
        config_.bopomofoFontAnnotationSupportEnabled.value()
            ? _("Bopomofo Font Annotation Support - On")
            : _("Bopomofo Font Annotation Support - Off"));
    bopomofoFontAnnotationSupportAction_->update(inputContext);
    inputContext->statusArea().addAction(
        fcitx::StatusGroup::InputMethod,
        bopomofoFontAnnotationSupportAction_.get());
  }

  if (mode == McBopomofo::InputMode::McBopomofo) {
    inputContext->statusArea().addAction(fcitx::StatusGroup::InputMethod,
                                         editUserPhrasesAction_.get());
    inputContext->statusArea().addAction(fcitx::StatusGroup::InputMethod,
                                         excludedPhrasesAction_.get());
  }
  // In both input modes.
  if (manageVocabularyAction_) {
    inputContext->statusArea().addAction(fcitx::StatusGroup::InputMethod,
                                         manageVocabularyAction_.get());
  }
  refreshFloatingCompositionAction(inputContext);

  keyHandler_->setInputMode(mode);

  keyHandler_->setKeyboardLayout(
      layoutForConfig(config_.bopomofoKeyboardLayout.value()));

  keyHandler_->setSelectPhraseAfterCursorAsCandidate(
      config_.selectPhrase.value() == SelectPhrase::AfterCursor);
  keyHandler_->setMoveCursorAfterSelection(
      config_.moveCursorAfterSelection.value());
  keyHandler_->setEscKeyClearsEntireComposingBuffer(
      config_.escKeyClearsEntireComposingBuffer.value());
  keyHandler_->setKeepInvalidSyllableForFurtherInput(
      config_.keepInvalidSyllableForFurtherInput.value());
  keyHandler_->setPutLowercaseLettersToComposingBuffer(
      config_.shiftLetterKeys.value() == ShiftLetterKeys::PutLowercaseToBuffer);
  keyHandler_->setShiftEnterEnabled(config_.shiftEnterEnabled.value());
  keyHandler_->setCtrlEnterKeyBehavior(config_.ctrlEnterKeys.value());
  keyHandler_->setAssociatedPhrasesEnabled(
      config_.associatedPhrasesEnabled.value());
  keyHandler_->setHalfWidthPunctuationEnabled(
      config_.halfWidthPunctuationEnable.value());
  keyHandler_->setRepeatedPunctuationToSelectCandidateEnabled(
      config_.repeatedPunctuationToSelectCandidateEnabled.value());
  keyHandler_->setChooseCandidateUsingSpace(
      config_.chooseCandidateUsingSpace.value());
  applySmartOptions();

  if (mode == McBopomofo::InputMode::McBopomofo) {
    // Font annotation is only supported in McBopomofo, not Plain McBopomofo.
    keyHandler_->setBopomofoFontAnnotationSupportEnabled(
        config_.bopomofoFontAnnotationSupportEnabled.value());
  } else {
    keyHandler_->setBopomofoFontAnnotationSupportEnabled(false);
  }

  userOverrideStore_->reloadIfChanged();
  // A removal made by the vocabulary manager or on another device applies at
  // once, rather than after the reload interval of the stores.
  if (removedEntries_->reloadIfChanged()) {
    englishWordStore_->reload();
    commonSentenceStore_->reload();
  }
  // Also drops the temporary phrases in Plain Bopomofo.
  temporaryPhraseStore_->reloadIfChanged();
  applyTemporaryPhrases();
  // The symbol table is read again if the file changed; Plain Bopomofo has
  // none.
  applySymbolTable();
  // The other devices' sentences, at most once a minute.
  commonSentenceStore_->reloadIfNeeded();

  bool didReload = languageModelLoader_->reloadUserModelsIfNeeded();
  if (didReload) {
    userFileIssues_ = languageModelLoader_->getUserFileIssues();
  }

  showUserDataFolderWarning();

  if (!userFileIssues_.empty()) {
    showAndClearUserFileIssues();
  }
}

void McBopomofoEngine::deactivate(const fcitx::InputMethodEntry& entry,
                                  fcitx::InputContextEvent& event) {
  // Commits what is being typed, which records the English words in it.
  fcitx::InputMethodEngine::deactivate(entry, event);
  capsLockKnown_ = false;
  saveLearning();
}

void McBopomofoEngine::reset(const fcitx::InputMethodEntry& /*unused*/,
                             fcitx::InputContextEvent& event) {
  fcitx::InputContext* context = event.inputContext();
  if (context == nullptr) {
    state_ = std::make_unique<InputStates::Empty>();
    return;
  }

  // Reset the UI.
  context->inputPanel().reset();
  context->updateUserInterface(fcitx::UserInterfaceComponent::InputPanel);

  // Explicitly reset the preedit when it's focus-out or reset. Force-commit
  // any residue upon all other reset events. McBopomofo users expect residue
  // to be committed upon IME switch. It would be ideal if focus-out or reset
  // also force-commits, but unfortunately different apps handle those two
  // events differently. Gnome Terminal force-commits any residue by itself but
  // Chrome clears everything. Some apps (e.g. Firefox) even rely on an explicit
  // preedit for those events to be handled correctly. Given how this is beyond
  // our control, the approach here is likely the most sensible.
  if (event.type() == fcitx::EventType::InputContextFocusOut ||
      event.type() == fcitx::EventType::InputContextReset) {
    keyHandler_->reset();

    bool useClientPreedit = this->useClientPreedit(context);
    if (useClientPreedit) {
      context->inputPanel().setClientPreedit(fcitx::Text{});
    } else {
      context->inputPanel().setPreedit(fcitx::Text{});
    }
    context->inputPanel().setAuxDown(fcitx::Text{});
    context->updatePreedit();
  } else {
    keyHandler_->handleForceCommitAndReset(
        [this, context](std::unique_ptr<InputState> next) {
          enterNewState(context, std::move(next));
        });
  }
  state_ = std::make_unique<InputStates::Empty>();
}

void McBopomofoEngine::keyEvent(const fcitx::InputMethodEntry& /*unused*/,
                                fcitx::KeyEvent& keyEvent) {
  if (!keyEvent.isInputContextEvent()) {
    return;
  }

  // The Caps Lock state only exists in the raw key. Whatever key event
  // arrives tells whether it changed.
  bool capsLockNow = keyEvent.rawKey().states() & fcitx::KeyState::CapsLock;
  bool capsLockWasKnown = capsLockKnown_;
  capsLockKnown_ = true;
  if (capsLockNow != capsLockOn_) {
    auto reminder = SmartLearning::SwitchReminderText(
        capsLockWasKnown, capsLockOn_, capsLockNow);
    capsLockOn_ = capsLockNow;
    updateChineseEnglishIcon(keyEvent.inputContext());
    if (reminder && config_.chineseEnglishSwitchReminderEnabled.value() &&
        notifications()) {
      notifications()->call<fcitx::INotifications::showTip>(
          "mcbopomofo-chinese-english-switch", _("McBopomofo"),
          "fcitx_mcbopomofo", *reminder, *reminder,
          kFcitx5NotificationTimeoutInMs);
    }
  }

  if (keyEvent.isRelease()) {
    return;
  }

  fcitx::InputContext* context = keyEvent.inputContext();
  fcitx::Key key = keyEvent.key();
  fcitx::Key origKey = keyEvent.rawKey();

  if (key.states() & fcitx::KeyState::Alt ||
      key.states() & fcitx::KeyState::Super) {
    keyHandler_->breakEscDoublePress();
    return;
  }

  // CapsLock is a special case. The state only exists in the raw key, not in
  // the key.
  if (origKey.states() & fcitx::KeyState::CapsLock) {
    if (!config_.capsLockAllowChineseInput.value()) {
      keyHandler_->reset();
      enterNewState(context, std::make_unique<InputStates::Empty>());
      return;
    }
  }

  InputStates::NumberInput* maybeNumberInput =
      dynamic_cast<InputStates::NumberInput*>(state_.get());
  if (maybeNumberInput != nullptr) {
    bool handled = keyHandler_->handleNumberInput(
        MapFcitxKey(key, origKey), maybeNumberInput,
        [this, context](std::unique_ptr<InputState> next) {
          enterNewState(context, std::move(next));
        },
        []() {
          // TODO(unassigned): beep?
        });
    if (handled) {
      keyEvent.filterAndAccept();
      return;
    }
  }

  InputStates::IcuTransformInput* maybeIcuTransformInput =
      dynamic_cast<InputStates::IcuTransformInput*>(state_.get());
  if (maybeIcuTransformInput != nullptr) {
    bool handled = keyHandler_->handleIcuTransformInput(
        MapFcitxKey(key, origKey), maybeIcuTransformInput,
        [this, context](std::unique_ptr<InputState> next) {
          enterNewState(context, std::move(next));
        },
        []() {
          // TODO(unassigned): beep?
        });
    if (handled) {
      keyEvent.filterAndAccept();
      return;
    }
  }

  // In the text templates, the keys go to the code first. Shift and a
  // selection key picks a template, but for an operator of a number, such as
  // Shift+8 for "*". The other keys go to the candidates, or are swallowed
  // if there are none.
  if (auto* textTemplate =
          dynamic_cast<InputStates::TextTemplate*>(state_.get())) {
    Key mappedKey = MapFcitxKey(key, origKey);
    bool picksCandidate =
        shiftedSelectionKeyIndex(key, origKey) != -1 &&
        !SmartLearning::IsNumberOperatorKey(textTemplate->code,
                                            mappedKey.ascii);
    if (!picksCandidate) {
      bool hasCandidates = !textTemplate->texts.empty();
      bool handled = keyHandler_->handleTextTemplateInput(
          mappedKey, textTemplate,
          [this, context](std::unique_ptr<InputState> next) {
            handleStateOrSequence(context, std::move(next));
          },
          []() {
            // TODO(unassigned): beep?
          });
      if (handled || !hasCandidates) {
        keyEvent.filterAndAccept();
        return;
      }
    }
  }

  if (dynamic_cast<InputStates::ChoosingCandidate*>(state_.get()) != nullptr ||
      dynamic_cast<InputStates::SelectingDictionary*>(state_.get()) !=
          nullptr ||
      dynamic_cast<InputStates::ShowingCharInfo*>(state_.get()) != nullptr ||
      dynamic_cast<InputStates::AssociatedPhrases*>(state_.get()) != nullptr ||
      dynamic_cast<InputStates::AssociatedPhrasesPlain*>(state_.get()) !=
          nullptr ||
      dynamic_cast<InputStates::SelectingFeature*>(state_.get()) != nullptr ||
      dynamic_cast<InputStates::SelectingDateMacro*>(state_.get()) != nullptr ||
      dynamic_cast<InputStates::CustomMenu*>(state_.get()) != nullptr ||
      dynamic_cast<InputStates::NumberInput*>(state_.get()) != nullptr ||
      dynamic_cast<InputStates::IcuTransformInput*>(state_.get()) != nullptr ||
      dynamic_cast<InputStates::TextTemplate*>(state_.get()) != nullptr) {
    // Absorb all keys when the candidate panel is on.
    keyEvent.filterAndAccept();

    auto* maybeCandidateList = dynamic_cast<fcitx::CommonCandidateList*>(
        context->inputPanel().candidateList().get());
    if (maybeCandidateList == nullptr) {
      // TODO(unassigned): Just assert this.
      FCITX_MCBOPOMOFO_WARN() << "inconsistent state";
      enterNewState(context, std::make_unique<InputStates::Empty>());
      context->updateUserInterface(fcitx::UserInterfaceComponent::InputPanel);
      context->updatePreedit();
      return;
    }

    keyHandler_->breakEscDoublePress();
    bool handled = handleCandidateKeyEvent(
        context, key, origKey, maybeCandidateList,
        [this, context](std::unique_ptr<InputState> next) {
          handleStateOrSequence(context, std::move(next));
        },
        []() {
          // TODO(unassigned): beep?
        });

    if (dynamic_cast<InputStates::ChoosingCandidate*>(state_.get()) !=
            nullptr ||
        dynamic_cast<InputStates::SelectingDictionary*>(state_.get()) !=
            nullptr ||
        dynamic_cast<InputStates::ShowingCharInfo*>(state_.get()) != nullptr ||
        dynamic_cast<InputStates::AssociatedPhrases*>(state_.get()) !=
            nullptr ||
        dynamic_cast<InputStates::AssociatedPhrasesPlain*>(state_.get()) !=
            nullptr ||
        dynamic_cast<InputStates::SelectingFeature*>(state_.get()) != nullptr ||
        dynamic_cast<InputStates::SelectingDateMacro*>(state_.get()) !=
            nullptr ||
        dynamic_cast<InputStates::CustomMenu*>(state_.get()) != nullptr ||
        dynamic_cast<InputStates::TextTemplate*>(state_.get()) != nullptr) {
      context->updateUserInterface(fcitx::UserInterfaceComponent::InputPanel);
      context->updatePreedit();
    }
    if (handled) {
      return;
    }
  }

  bool accepted = keyHandler_->handle(
      MapFcitxKey(key, origKey), state_.get(),
      [this, context](std::unique_ptr<InputState> next) {
        handleStateOrSequence(context, std::move(next));
      },
      []() {
        // TODO(unassigned): beep?
      });

  if (accepted) {
    keyEvent.filterAndAccept();
    return;
  }
}

void McBopomofoEngine::returnToCandidateList(
    fcitx::InputContext* context,
    const McBopomofo::KeyHandler::StateCallback& stateCallback,
    const InputStates::ChoosingCandidate& choosing, size_t selectedIndex) {
  // Entering the new state may destroy the menu state that |choosing| and
  // |stateCallback| belong to, so nothing of them is used afterwards.
  size_t candidateCount = choosing.candidates.size();
  stateCallback(std::make_unique<InputStates::ChoosingCandidate>(choosing));
  auto* list = dynamic_cast<fcitx::CommonCandidateList*>(
      context->inputPanel().candidateList().get());
  if (list == nullptr || selectedIndex >= candidateCount) {
    return;
  }
  // Make sure fcitx5 shows the page of the candidate; needed when page > 0.
  list->setPage(static_cast<int>(SmartLearning::PageOfCandidateIndex(
      selectedIndex, static_cast<size_t>(list->pageSize()))));
  list->setGlobalCursorIndex(static_cast<int>(selectedIndex));
}

bool McBopomofoEngine::handleCandidateKeyEvent(
    fcitx::InputContext* context, fcitx::Key key, fcitx::Key origKey,
    fcitx::CommonCandidateList* candidateList,
    const McBopomofo::KeyHandler::StateCallback& stateCallback,
    const McBopomofo::KeyHandler::ErrorCallback& errorCallback) {
  InputStates::AssociatedPhrases* associatedPhrases =
      dynamic_cast<InputStates::AssociatedPhrases*>(state_.get());
  InputStates::AssociatedPhrasesPlain* associatedPhrasesPlain =
      dynamic_cast<InputStates::AssociatedPhrasesPlain*>(state_.get());
  InputStates::NumberInput* numberInput =
      dynamic_cast<InputStates::NumberInput*>(state_.get());
  InputStates::IcuTransformInput* icuTransformInput =
      dynamic_cast<InputStates::IcuTransformInput*>(state_.get());
  InputStates::ChoosingPunctuationList* choosingPunctuationList =
      dynamic_cast<InputStates::ChoosingPunctuationList*>(state_.get());
  InputStates::TextTemplate* textTemplate =
      dynamic_cast<InputStates::TextTemplate*>(state_.get());

  if (choosingPunctuationList != nullptr && symbolGrid_ != nullptr) {
    return handleSymbolTableKeyEvent(context, key, origKey, stateCallback,
                                     errorCallback);
  }

  if (associatedPhrases != nullptr && associatedPhrases->autoTriggered) {
    if (key.check(FcitxKey_Tab)) {
      auto expanded = std::make_unique<InputStates::AssociatedPhrases>(
          std::move(associatedPhrases->previousState),
          associatedPhrases->prefixCursorIndex,
          associatedPhrases->prefixReading, associatedPhrases->prefixValue,
          associatedPhrases->selectedCandidateIndex,
          associatedPhrases->candidates, false);
      stateCallback(std::move(expanded));
      return true;
    }
    if (key.check(FcitxKey_Return, fcitx::KeyStates(fcitx::KeyState::Shift)) ||
        key.check(FcitxKey_KP_Enter,
                  fcitx::KeyStates(fcitx::KeyState::Shift))) {
      int idx = candidateList->cursorIndex();
      if (idx < candidateList->size()) {
        candidateList->candidate(idx).select(context);
      }
      return true;
    }
    stateCallback(keyHandler_->buildInputtingState());
    return false;
  }

  bool shouldUseShiftKey = associatedPhrasesPlain != nullptr;

  if (textTemplate != nullptr) {
    // Shift and a selection key, for any selection keys; a key past the
    // candidates of the page is swallowed.
    int idx = shiftedSelectionKeyIndex(key, origKey);
    if (idx != -1) {
      if (idx < candidateList->size()) {
        candidateList->candidate(idx).select(context);
      }
      return true;
    }
  } else if (shouldUseShiftKey || numberInput != nullptr ||
             icuTransformInput != nullptr) {
    // Plain Bopomofo, Associated Phrases, and Number Input.
    int code = origKey.code();
    // Shift-[1-9] keys can only be checked via raw key codes. The Key objects
    // in the selectionKeys_ do not carry such information.
    if ((origKey.states() & fcitx::KeyState::Shift) &&
        code >= kFcitxRawKeycode_1 && code <= kFcitxRawKeycode_9) {
      int idx = code - kFcitxRawKeycode_1;
      if (idx < candidateList->size()) {
        candidateList->candidate(idx).select(context);
      }
      return true;
    }
  } else {
    // handle num pad.

    int idx = key.keyListIndex(selectionKeys_);
    if (idx == -1) {
      idx = key.keyListIndex(numpadSelectionKeys_);
    }

    if (idx != -1 && idx < candidateList->size()) {
      candidateList->candidate(idx).select(context);
      return true;
    }
  }

  MovingCursorOption movingCursorOption =
      config_.allowMovingCursorWhenChoosingCandidates.value();

  bool isCursorMovingLeft = false;
  bool isCursorMovingRight = false;

  {
    // Note:
    // - The symbol table, where the cursor does not move, is handled above.
    // - Shift + Left/Right is the default keybinding for moving cursor in
    //   candidate list. When movingCursorOption is enabled, we also allow using
    //   H/J/K/L or Left/Right without modifiers
    isCursorMovingLeft =
        key.check(FcitxKey_Left, fcitx::KeyStates(fcitx::KeyState::Shift));
    isCursorMovingRight =
        key.check(FcitxKey_Right, fcitx::KeyStates(fcitx::KeyState::Shift));
    if (!isCursorMovingLeft && !isCursorMovingRight) {
      if (movingCursorOption == MovingCursorOption::UseJK) {
        isCursorMovingLeft = key.check(FcitxKey_j);
        isCursorMovingRight = key.check(FcitxKey_k);
      } else if (movingCursorOption == MovingCursorOption::UseHL) {
        isCursorMovingLeft = key.check(FcitxKey_h);
        isCursorMovingRight = key.check(FcitxKey_l);
      }
    }

    if (keyHandler_->inputMode() == McBopomofo::InputMode::McBopomofo &&
        dynamic_cast<InputStates::ChoosingCandidate*>(state_.get()) !=
            nullptr &&
        (isCursorMovingLeft || isCursorMovingRight)) {
      size_t cursor = keyHandler_->candidateCursorIndex();

      if (isCursorMovingLeft) {
        if (cursor > 0) {
          cursor--;
        }
      } else if (isCursorMovingRight) {
        cursor++;
      }
      keyHandler_->setCandidateCursorIndex(cursor);
      auto inputting = keyHandler_->buildInputtingState();
      auto choosing = keyHandler_->buildChoosingCandidateState(
          inputting.get(), keyHandler_->candidateCursorIndex());
      stateCallback(std::move(choosing));
      return true;
    }
  }

  bool keyIsCancel = false;

  std::vector<std::string> invalidPrefixes = {
      "_half_punctuation_", "_ctrl_punctuation_", "_letter_",
      "_number_",           "_punctuation_",
  };

  // When pressing "?" in the candidate list, tries to look up the candidate
  // in dictionaries.
  if (keyHandler_->inputMode() == McBopomofo::InputMode::McBopomofo &&
      key.check(FcitxKey_question)) {
    auto* choosingCandidate =
        dynamic_cast<InputStates::ChoosingCandidate*>(state_.get());
    auto* selectingDictionary =
        dynamic_cast<InputStates::SelectingDictionary*>(state_.get());
    auto* showingCharInfo =
        dynamic_cast<InputStates::ShowingCharInfo*>(state_.get());

    if (choosingCandidate != nullptr && choosingPunctuationList == nullptr) {
      // Enter selecting dictionary service state.
      if (keyHandler_->hasDictionaryServices()) {
        size_t selectedCandidateIndex = candidateList->globalCursorIndex();
        if (selectedCandidateIndex >= choosingCandidate->candidates.size()) {
          errorCallback();
          return true;
        }
        std::string reading =
            choosingCandidate->candidates[selectedCandidateIndex].reading;

        // If the reading has an invalid prefix, skip dictionary lookup
        if (std::any_of(invalidPrefixes.begin(), invalidPrefixes.end(),
                        [&reading](const std::string& prefix) {
                          return reading.compare(0, prefix.length(), prefix) ==
                                 0;
                        })) {
          return true;
        }

        std::string phrase =
            choosingCandidate->candidates[selectedCandidateIndex].value;
        std::unique_ptr<InputStates::ChoosingCandidate> copy =
            std::make_unique<InputStates::ChoosingCandidate>(
                *choosingCandidate);
        auto newState = keyHandler_->buildSelectingDictionaryState(
            std::move(copy), phrase, selectedCandidateIndex);
        stateCallback(std::move(newState));
        return true;
      }
    } else if (selectingDictionary != nullptr || showingCharInfo != nullptr) {
      // Leave selecting dictionary service state.
      keyIsCancel = true;
    }
  }

  if (keyHandler_->inputMode() == McBopomofo::InputMode::McBopomofo) {
    auto* choosingCandidate =
        dynamic_cast<InputStates::ChoosingCandidate*>(state_.get());
    bool isPlusKey = key.check(FcitxKey_plus) || key.check(FcitxKey_equal);
    bool isMinusKey =
        key.check(FcitxKey_minus) || key.check(FcitxKey_underscore);
    if (choosingCandidate != nullptr && (isPlusKey || isMinusKey)) {
      size_t selectedCandidateIndex = candidateList->globalCursorIndex();
      if (selectedCandidateIndex >= choosingCandidate->candidates.size()) {
        errorCallback();
        return true;
      }
      auto candidate = choosingCandidate->candidates[selectedCandidateIndex];
      std::string reading = candidate.reading;
      std::string phrase = candidate.value;
      switch (SmartLearning::CheckCandidateAdjustable(reading, phrase,
                                                      candidate.rawValue)) {
        case SmartLearning::AdjustCheck::kIgnoreSilently:
          return true;
        case SmartLearning::AdjustCheck::kRejectWithError:
          errorCallback();
          return true;
        case SmartLearning::AdjustCheck::kAllowed:
          break;
      }

      std::vector<InputStates::CustomMenu::MenuEntry> entries;
      std::string title;
      if (isPlusKey) {
        InputStates::CustomMenu::MenuEntry confirmEntry(
            _("Boost"), [this, phrase, reading, stateCallback]() {
              keyHandler_->boostPhrase(reading, phrase);
              auto inputting = keyHandler_->buildInputtingState();
              stateCallback(std::move(inputting));
            });
        entries.emplace_back(std::move(confirmEntry));
        title = fmt::format(
            FmtRuntime(
                _("Do you want to boost the score of the phrase \"{}\"?")),
            phrase);
      } else {
        InputStates::CustomMenu::MenuEntry confirmEntry(
            _("Exclude"), [this, phrase, reading, stateCallback]() {
              keyHandler_->excludePhrase(reading, phrase);
              auto inputting = keyHandler_->buildInputtingState();
              stateCallback(std::move(inputting));
            });
        entries.emplace_back(std::move(confirmEntry));
        title = fmt::format(
            FmtRuntime(_("Do you want to exclude the phrase \"{}\"?")),
            phrase);
      }

      // Cancelling goes back to the candidate list, with the same candidate
      // highlighted.
      std::shared_ptr<InputStates::ChoosingCandidate> previous =
          std::make_shared<InputStates::ChoosingCandidate>(*choosingCandidate);
      InputStates::CustomMenu::MenuEntry cancelEntry(
          _("Cancel"), [this, context, stateCallback, previous,
                        selectedCandidateIndex]() {
            returnToCandidateList(context, stateCallback, *previous,
                                  selectedCandidateIndex);
          });
      entries.push_back(std::move(cancelEntry));
      auto copy =
          std::make_unique<InputStates::ChoosingCandidate>(*choosingCandidate);
      auto confirm = std::make_unique<InputStates::CustomMenu>(
          std::move(copy), std::move(title), std::move(entries));
      confirm->selectedCandidateIndex = selectedCandidateIndex;
      stateCallback(std::move(confirm));
      return true;
    }
  }

  if (keyHandler_->inputMode() == McBopomofo::InputMode::McBopomofo &&
      !shouldUseShiftKey && origKey.code() == kFcitxRawKeycode_Enter &&
      (origKey.states() & fcitx::KeyState::Shift)) {
    int idx = candidateList->cursorIndex();
    if (idx < candidateList->size()) {
      auto* choosingCandidate =
          dynamic_cast<InputStates::ChoosingCandidate*>(state_.get());
      if (choosingCandidate != nullptr) {
        size_t globalIndex = candidateList->globalCursorIndex();
        auto prevState = std::make_unique<InputStates::ChoosingCandidate>(
            *choosingCandidate);
        std::string prefixReading =
            choosingCandidate->candidates[globalIndex].reading;
        std::string prefixValue =
            choosingCandidate->candidates[globalIndex].value;

        std::unique_ptr<InputStates::AssociatedPhrases> newState =
            keyHandler_->buildAssociatedPhrasesStateFromCandidateChoosingState(
                std::move(prevState), choosingCandidate->originalCursor,
                prefixReading, prefixValue, globalIndex);
        if (newState != nullptr) {
          stateCallback(std::move(newState));
        }
        return true;
      }
    }
  }

  bool returnPressed = false;
  if (shouldUseShiftKey) {
    // If it is the associated phrases mode, we need to check if the shift key
    // is pressed.
    returnPressed =
        key.check(FcitxKey_Return, fcitx::KeyStates(fcitx::KeyState::Shift));
  } else if (textTemplate != nullptr) {
    // A text template is committed with Enter, with Shift or not.
    returnPressed =
        key.check(FcitxKey_Return) || key.check(FcitxKey_KP_Enter) ||
        key.check(FcitxKey_Return, fcitx::KeyStates(fcitx::KeyState::Shift)) ||
        key.check(FcitxKey_KP_Enter, fcitx::KeyStates(fcitx::KeyState::Shift));
  } else {
    // Otherwise, we just check if the return key is pressed.
    returnPressed = key.check(FcitxKey_Return);
  }

  if (returnPressed) {
    int idx = candidateList->cursorIndex();
    if (idx < candidateList->size()) {
      candidateList->candidate(idx).select(context);
    }
    return true;
  }

  if (keyIsCancel || key.check(FcitxKey_Escape) ||
      key.check(FcitxKey_BackSpace)) {
    auto* showingCharInfo =
        dynamic_cast<InputStates::ShowingCharInfo*>(state_.get());
    if (showingCharInfo != nullptr) {
      auto* previous = showingCharInfo->previousState.get();
      auto copy = std::make_unique<InputStates::SelectingDictionary>(*previous);
      stateCallback(std::move(copy));
      return true;
    }

    auto* customMenu = dynamic_cast<InputStates::CustomMenu*>(state_.get());
    if (customMenu != nullptr) {
      auto* choosingCandidate = dynamic_cast<InputStates::ChoosingCandidate*>(
          customMenu->previousState.get());
      if (choosingCandidate != nullptr) {
        returnToCandidateList(context, stateCallback, *choosingCandidate,
                              customMenu->selectedCandidateIndex);
      }
      return true;
    }

    auto* selecting =
        dynamic_cast<InputStates::SelectingDictionary*>(state_.get());
    if (selecting != nullptr) {
      auto* previous = selecting->previousState.get();
      auto* choosing = dynamic_cast<InputStates::ChoosingCandidate*>(previous);
      if (choosing != nullptr) {
        auto copy = std::make_unique<InputStates::ChoosingCandidate>(*choosing);
        stateCallback(std::move(copy));

        auto* maybeCandidateList = dynamic_cast<fcitx::CommonCandidateList*>(
            context->inputPanel().candidateList().get());
        size_t index = selecting->selectedCandidateIndex;
        // Make sure fcitx5 shows the candidate page; needed when page > 0.
        maybeCandidateList->setPage(static_cast<int>(index) /
                                    maybeCandidateList->pageSize());
        maybeCandidateList->setGlobalCursorIndex(static_cast<int>(index));
        return true;
      }
      auto* marking = dynamic_cast<InputStates::Marking*>(previous);
      if (marking != nullptr) {
        auto copy = std::make_unique<InputStates::Marking>(*marking);
        stateCallback(std::move(copy));
      }
      return true;
    }

    if (associatedPhrases != nullptr) {
      auto* previous = associatedPhrases->previousState.get();
      auto* choosing = dynamic_cast<InputStates::ChoosingCandidate*>(previous);
      auto* inputting = dynamic_cast<InputStates::Inputting*>(previous);
      if (choosing != nullptr) {
        auto copy = std::make_unique<InputStates::ChoosingCandidate>(*choosing);
        stateCallback(std::move(copy));

        auto* maybeCandidateList = dynamic_cast<fcitx::CommonCandidateList*>(
            context->inputPanel().candidateList().get());
        size_t index = associatedPhrases->selectedCandidateIndex;

        // Make sure fcitx5 shows the candidate page; needed when page > 0.
        maybeCandidateList->setPage(static_cast<int>(index) /
                                    maybeCandidateList->pageSize());
        maybeCandidateList->setGlobalCursorIndex(static_cast<int>(index));
      } else if (inputting != nullptr) {
        auto copy = std::make_unique<InputStates::Inputting>(*inputting);
        stateCallback(std::move(copy));
      } else {
        auto inputting = keyHandler_->buildInputtingState();
        stateCallback(std::move(inputting));
      }
      return true;
    }

    size_t originalCursor = 0;
    auto* choosing =
        dynamic_cast<InputStates::ChoosingCandidate*>(state_.get());
    if (choosing != nullptr) {
      originalCursor = choosing->originalCursor;
    }

    keyHandler_->candidatePanelCancelled(
        originalCursor, [stateCallback](std::unique_ptr<InputState> next) {
          stateCallback(std::move(next));
        });
    return true;
  }

  fcitx::CandidateLayoutHint layoutHint = getCandidateLayoutHint();
  candidateList->setLayoutHint(layoutHint);

  // Space goes to next page or wraps to the first if at the end.
  if (key.check(FcitxKey_space)) {
    if (candidateList->hasNext()) {
      candidateList->next();
      candidateList->toCursorMovable()->nextCandidate();
    } else if (candidateList->currentPage() > 0) {
      candidateList->setPage(0);
      candidateList->toCursorMovable()->nextCandidate();
    }
    context->updateUserInterface(fcitx::UserInterfaceComponent::InputPanel);
    return true;
  }

  bool isAdditionalPageUp = false;
  bool isAdditionalPageDown = false;

  if (movingCursorOption == MovingCursorOption::UseJK) {
    isAdditionalPageUp = key.check(FcitxKey_h);
    isAdditionalPageDown = key.check(FcitxKey_l);
  } else if (movingCursorOption == MovingCursorOption::UseHL) {
    isAdditionalPageUp = key.check(FcitxKey_j);
    isAdditionalPageDown = key.check(FcitxKey_k);
  }

  if (isAdditionalPageDown) {
    if (candidateList->hasNext()) {
      candidateList->next();
      candidateList->toCursorMovable()->nextCandidate();
    } else if (candidateList->currentPage() > 0) {
      candidateList->setPage(0);
      candidateList->toCursorMovable()->nextCandidate();
    }
    context->updateUserInterface(fcitx::UserInterfaceComponent::InputPanel);
    return true;
  }

  if (isAdditionalPageUp) {
    if (candidateList->hasPrev()) {
      candidateList->prev();
      candidateList->toCursorMovable()->nextCandidate();
    } else {
      int totalPages = candidateList->totalPages();
      if (totalPages > 0) {
        candidateList->setPage(totalPages - 1);
      }
      candidateList->toCursorMovable()->nextCandidate();
    }
    context->updateUserInterface(fcitx::UserInterfaceComponent::InputPanel);
    return true;
  }

  bool isVertical = (layoutHint == fcitx::CandidateLayoutHint::Vertical);

  if (isVertical) {
    if (key.check(fcitx::Key(FcitxKey_Down))) {
      candidateList->toCursorMovable()->nextCandidate();
      context->updateUserInterface(fcitx::UserInterfaceComponent::InputPanel);
      return true;
    }
    if (key.check(fcitx::Key(FcitxKey_Up))) {
      candidateList->toCursorMovable()->prevCandidate();
      context->updateUserInterface(fcitx::UserInterfaceComponent::InputPanel);
      return true;
    }
    if (key.check(fcitx::Key(FcitxKey_Right)) ||
        key.check(fcitx::Key(FcitxKey_Page_Down)) ||
        key.check(fcitx::Key(FcitxKey_KP_Right)) ||
        key.check(fcitx::Key(FcitxKey_KP_Page_Down)) ||
        key.checkKeyList(instance_->globalConfig().defaultNextPage())) {
      if (candidateList->hasNext()) {
        candidateList->next();
        candidateList->toCursorMovable()->nextCandidate();
      } else if (candidateList->currentPage() > 0) {
        candidateList->setPage(0);
        candidateList->toCursorMovable()->nextCandidate();
      }
      context->updateUserInterface(fcitx::UserInterfaceComponent::InputPanel);
      return true;
    }
    if (key.check(fcitx::Key(FcitxKey_Left)) ||
        key.check(fcitx::Key(FcitxKey_Page_Up)) ||
        key.check(fcitx::Key(FcitxKey_KP_Left)) ||
        key.check(fcitx::Key(FcitxKey_KP_Page_Up)) ||
        key.checkKeyList(instance_->globalConfig().defaultPrevPage())) {
      if (candidateList->hasPrev()) {
        candidateList->prev();
        candidateList->toCursorMovable()->nextCandidate();
      } else {
        int totalPages = candidateList->totalPages();
        if (totalPages > 0) {
          candidateList->setPage(totalPages - 1);
        }
        candidateList->toCursorMovable()->nextCandidate();
      }
      context->updateUserInterface(fcitx::UserInterfaceComponent::InputPanel);
      return true;
    }
  } else {
    if (key.check(fcitx::Key(FcitxKey_Right)) ||
        key.check(fcitx::Key(FcitxKey_KP_Right))) {
      candidateList->toCursorMovable()->nextCandidate();
      context->updateUserInterface(fcitx::UserInterfaceComponent::InputPanel);
      return true;
    }
    if (key.check(fcitx::Key(FcitxKey_Left)) ||
        key.check(fcitx::Key(FcitxKey_KP_Left))) {
      candidateList->toCursorMovable()->prevCandidate();
      context->updateUserInterface(fcitx::UserInterfaceComponent::InputPanel);
      return true;
    }
    if (key.check(fcitx::Key(FcitxKey_Down)) ||
        key.check(fcitx::Key(FcitxKey_KP_Down)) ||
        key.check(fcitx::Key(FcitxKey_Page_Down)) ||
        key.check(fcitx::Key(FcitxKey_KP_Page_Down)) ||
        key.checkKeyList(instance_->globalConfig().defaultNextPage())) {
      if (candidateList->hasNext()) {
        candidateList->next();
        candidateList->toCursorMovable()->nextCandidate();
      } else if (candidateList->currentPage() > 0) {
        candidateList->setPage(0);
        candidateList->toCursorMovable()->nextCandidate();
      }
      context->updateUserInterface(fcitx::UserInterfaceComponent::InputPanel);
      return true;
    }
    if (key.check(fcitx::Key(FcitxKey_Up)) ||
        key.check(fcitx::Key(FcitxKey_KP_Up)) ||
        key.check(fcitx::Key(FcitxKey_Page_Up)) ||
        key.check(fcitx::Key(FcitxKey_KP_Page_Up)) ||
        key.checkKeyList(instance_->globalConfig().defaultPrevPage())) {
      if (candidateList->hasPrev()) {
        candidateList->prev();
        candidateList->toCursorMovable()->nextCandidate();
      } else {
        int totalPages = candidateList->totalPages();
        if (totalPages > 0) {
          candidateList->setPage(totalPages - 1);
        }
        candidateList->toCursorMovable()->nextCandidate();
      }
      context->updateUserInterface(fcitx::UserInterfaceComponent::InputPanel);
      return true;
    }
  }

  if (associatedPhrases != nullptr) {
    if (!origKey.isModifier()) {
      return false;
    }
  }

  if (associatedPhrasesPlain != nullptr) {
    if (!origKey.isModifier()) {
      std::unique_ptr<InputStates::Empty> empty =
          std::make_unique<InputStates::Empty>();
      stateCallback(std::move(empty));
      return false;
    }
  }

  if (dynamic_cast<InputStates::ChoosingCandidate*>(state_.get()) != nullptr) {
    bool result =
        keyHandler_->handleCandidateKeyForTraditionalBopomofoIfRequired(
            MapFcitxKey(key, origKey),
            [candidateList, context] {
              auto idx = candidateList->cursorIndex();
              if (idx < candidateList->size()) {
                candidateList->candidate(idx).select(context);
              }
            },
            stateCallback, errorCallback);

    if (result) {
      return true;
    }
  }

  // TODO(unassigned): All else... beep?
  errorCallback();
  return true;
}

bool McBopomofoEngine::handleSymbolTableKeyEvent(
    fcitx::InputContext* context, fcitx::Key key, fcitx::Key origKey,
    const McBopomofo::KeyHandler::StateCallback& stateCallback,
    const McBopomofo::KeyHandler::ErrorCallback& errorCallback) {
  auto* symbolTable =
      dynamic_cast<InputStates::ChoosingPunctuationList*>(state_.get());
  SmartLearning::SymbolGrid& grid = *symbolGrid_;
  size_t originalCursor = symbolTable->originalCursor;
  // Picking a symbol leaves this state, so nothing of it is used after.
  auto pick = [&](size_t index) {
    if (index >= symbolTable->candidates.size()) {
      errorCallback();
      return;
    }
    InputStates::ChoosingCandidate::Candidate candidate =
        symbolTable->candidates[index];
    keyHandler_->candidateSelected(candidate, originalCursor, stateCallback);
  };
  auto showOrBeep = [&](bool moved) {
    if (moved) {
      showSymbolTable(context);
    } else {
      errorCallback();
    }
  };

  // The selection keys come first, so that a selection key that is also a
  // punctuation key, such as ";", picks a row or a symbol.
  int idx = key.keyListIndex(selectionKeys_);
  if (idx == -1) {
    idx = key.keyListIndex(numpadSelectionKeys_);
  }
  // A letter key matches ignoring case, such as Shift+D for "d".
  if (idx == -1 &&
      !(key.states() & fcitx::KeyStates{fcitx::KeyState::Ctrl,
                                        fcitx::KeyState::Alt,
                                        fcitx::KeyState::Super})) {
    auto sym = key.sym();
    char letter = 0;
    if (sym >= FcitxKey_A && sym <= FcitxKey_Z) {
      letter = static_cast<char>('a' + (sym - FcitxKey_A));
    } else if (sym >= FcitxKey_a && sym <= FcitxKey_z) {
      letter = static_cast<char>('a' + (sym - FcitxKey_a));
    }
    if (auto index = SmartLearning::SelectionKeyIndexIgnoringCase(
            selectionKeyCharacters(), letter)) {
      idx = static_cast<int>(*index);
    }
  }
  if (idx != -1) {
    if (grid.isChoosingRow()) {
      showOrBeep(grid.chooseRow(static_cast<size_t>(idx)));
    } else if (auto index = grid.candidateAt(static_cast<size_t>(idx))) {
      pick(*index);
    } else {
      errorCallback();
    }
    return true;
  }

  bool isDelete = key.check(FcitxKey_BackSpace) ||
                  key.check(FcitxKey_Delete) || key.check(FcitxKey_KP_Delete);
  // Backspace after picking a row goes back to the rows.
  if (!grid.isChoosingRow() && isDelete) {
    grid.cancelChoosingCell();
    showSymbolTable(context);
    return true;
  }
  if (key.check(FcitxKey_grave)) {
    keyHandler_->candidatePanelFeatureMenuRequested(stateCallback);
    return true;
  }
  // Another punctuation key shows its symbols, followed by the table.
  if (keyHandler_->candidatePanelPunctuationMaybeEntered(
          MapFcitxKey(key, origKey), originalCursor, stateCallback)) {
    return true;
  }
  if (key.check(FcitxKey_Escape) || isDelete) {
    keyHandler_->candidatePanelPunctuationListCancelled(originalCursor,
                                                        stateCallback);
    return true;
  }
  if (key.check(FcitxKey_Return) || key.check(FcitxKey_KP_Enter)) {
    pick(grid.highlighted());
    return true;
  }

  // The arrow keys move the highlight in their directions.
  if (key.check(FcitxKey_Left) || key.check(FcitxKey_KP_Left)) {
    showOrBeep(grid.highlightPrevious());
    return true;
  }
  if (key.check(FcitxKey_Right) || key.check(FcitxKey_KP_Right)) {
    showOrBeep(grid.highlightNext());
    return true;
  }
  if (key.check(FcitxKey_Up) || key.check(FcitxKey_KP_Up)) {
    showOrBeep(grid.moveByRows(-1));
    return true;
  }
  if (key.check(FcitxKey_Down) || key.check(FcitxKey_KP_Down)) {
    showOrBeep(grid.moveByRows(1));
    return true;
  }
  if (key.check(FcitxKey_Home) || key.check(FcitxKey_KP_Home)) {
    showOrBeep(grid.highlightFirst());
    return true;
  }
  if (key.check(FcitxKey_End) || key.check(FcitxKey_KP_End)) {
    showOrBeep(grid.highlightLast());
    return true;
  }

  MovingCursorOption movingCursorOption =
      config_.allowMovingCursorWhenChoosingCandidates.value();
  size_t rowsPerPage = std::max<size_t>(selectionKeys_.size(), 1);
  bool isPageDown =
      key.check(FcitxKey_space) || key.check(FcitxKey_Page_Down) ||
      key.check(FcitxKey_KP_Page_Down) ||
      key.checkKeyList(instance_->globalConfig().defaultNextPage()) ||
      (movingCursorOption == MovingCursorOption::UseJK &&
       key.check(FcitxKey_l)) ||
      (movingCursorOption == MovingCursorOption::UseHL &&
       key.check(FcitxKey_k));
  bool isPageUp =
      key.check(FcitxKey_Page_Up) || key.check(FcitxKey_KP_Page_Up) ||
      key.checkKeyList(instance_->globalConfig().defaultPrevPage()) ||
      (movingCursorOption == MovingCursorOption::UseJK &&
       key.check(FcitxKey_h)) ||
      (movingCursorOption == MovingCursorOption::UseHL &&
       key.check(FcitxKey_j));
  if (isPageDown) {
    showOrBeep(grid.nextPage(rowsPerPage));
    return true;
  }
  if (isPageUp) {
    showOrBeep(grid.previousPage(rowsPerPage));
    return true;
  }

  // Plain Bopomofo picks the highlighted symbol and goes on with the key.
  if (keyHandler_->handleCandidateKeyForTraditionalBopomofoIfRequired(
          MapFcitxKey(key, origKey), [&] { pick(grid.highlighted()); },
          stateCallback, errorCallback)) {
    return true;
  }

  errorCallback();
  return true;
}

void McBopomofoEngine::showSymbolTable(fcitx::InputContext* context) {
  auto* symbolTable =
      dynamic_cast<InputStates::ChoosingPunctuationList*>(state_.get());
  if (symbolTable == nullptr || symbolGrid_ == nullptr) {
    return;
  }
  const SmartLearning::SymbolGrid& grid = *symbolGrid_;
  const auto& rows = grid.rows();
  auto candidateList = std::make_unique<fcitx::CommonCandidateList>();
  candidateList->setSelectionKey(selectionKeys_);
  candidateList->setPageSize(
      static_cast<int>(std::max<size_t>(selectionKeys_.size(), 1)));
  KeyHandler::StateCallback callback =
      [this, context](std::unique_ptr<InputState> next) {
        handleStateOrSequence(context, std::move(next));
      };

  int cursor = 0;
  if (grid.isChoosingRow()) {
    // The rows labeled with the keys, from the highlighted one down, each
    // shown as its title and symbols; the highlighted symbol is underlined.
    candidateList->setLayoutHint(fcitx::CandidateLayoutHint::Vertical);
    auto [first, last] = grid.labeledRowRange();
    for (size_t r = first; r < last; ++r) {
      const auto& row = rows[r];
      fcitx::Text text;
      if (!row.title.empty()) {
        text.append(row.title + "  ");
      }
      for (size_t i = row.start; i < row.start + row.count; ++i) {
        if (i > row.start) {
          text.append(" ");
        }
        const std::string& value = symbolTable->candidates[i].value;
        if (i == grid.highlighted()) {
          text.append(value, fcitx::TextFormatFlags{
                                 fcitx::TextFormatFlag::Underline} |
                                 fcitx::TextFormatFlag::Bold);
        } else {
          text.append(value);
        }
      }
      candidateList->append(std::make_unique<McBopomofoSymbolRowWord>(
          std::move(text), [this, r](fcitx::InputContext* ic) {
            // showSymbolTable() destroys this row and this function; nothing
            // may follow it.
            if (symbolGrid_ != nullptr && symbolGrid_->chooseRowAt(r)) {
              showSymbolTable(ic);
            }
          }));
    }
  } else if (!rows.empty()) {
    // The symbols of the row picked, labeled from its first one.
    candidateList->setLayoutHint(fcitx::CandidateLayoutHint::Horizontal);
    const auto& row = rows[grid.highlightedRow()];
    for (size_t i = row.start; i < row.start + row.count; ++i) {
      const auto& c = symbolTable->candidates[i];
      candidateList->append(std::make_unique<McBopomofoCandidateWord>(
          fcitx::Text(c.value), c, symbolTable->originalCursor, keyHandler_,
          callback));
    }
    cursor = static_cast<int>(grid.highlighted() - row.start);
  }
  if (candidateList->size() > 0) {
    candidateList->setGlobalCursorIndex(cursor);
  }
  context->inputPanel().reset();
  context->inputPanel().setCandidateList(std::move(candidateList));
  context->updateUserInterface(fcitx::UserInterfaceComponent::InputPanel);
  updatePreedit(context, symbolTable);
}

std::string McBopomofoEngine::selectionKeyCharacters() const {
  switch (config_.selectionKeys.value()) {
    case SelectionKeys::Key_asdfghjkl:
      return "asdfghjkl";
    case SelectionKeys::Key_asdfzxcvb:
      return "asdfzxcvb";
    default:
      return "123456789";
  }
}

int McBopomofoEngine::shiftedSelectionKeyIndex(const fcitx::Key& key,
                                               const fcitx::Key& origKey) const {
  if (!(origKey.states() & fcitx::KeyState::Shift) ||
      (origKey.states() & fcitx::KeyState::Ctrl)) {
    return -1;
  }
  std::string keys = selectionKeyCharacters();
  if (keys == "123456789") {
    // Shift and a digit can only be told by the raw key code.
    int code = origKey.code();
    if (code >= kFcitxRawKeycode_1 && code <= kFcitxRawKeycode_9) {
      return code - kFcitxRawKeycode_1;
    }
    return -1;
  }
  char letter = 0;
  auto sym = key.sym();
  if (sym >= FcitxKey_A && sym <= FcitxKey_Z) {
    letter = static_cast<char>('a' + (sym - FcitxKey_A));
  } else if (sym >= FcitxKey_a && sym <= FcitxKey_z) {
    letter = static_cast<char>('a' + (sym - FcitxKey_a));
  }
  size_t index = letter != 0 ? keys.find(letter) : std::string::npos;
  return index == std::string::npos ? -1 : static_cast<int>(index);
}

void McBopomofoEngine::enterNewState(fcitx::InputContext* context,
                                     std::unique_ptr<InputState> newState) {
  // Hold the previous state, and transfer the ownership of newState.
  std::unique_ptr<InputState> prevState = std::move(state_);
  state_ = std::move(newState);

  InputState* prevPtr = prevState.get();
  InputState* currentPtr = state_.get();

  if (dynamic_cast<InputStates::ChoosingPunctuationList*>(currentPtr) ==
      nullptr) {
    symbolGrid_.reset();
  }

  if (auto* empty = dynamic_cast<InputStates::Empty*>(currentPtr)) {
    handleEmptyState(context, prevPtr, empty);

  } else if (auto* emptyIgnoringPrevious =
                 dynamic_cast<InputStates::EmptyIgnoringPrevious*>(
                     currentPtr)) {
    handleEmptyIgnoringPreviousState(context, prevPtr, emptyIgnoringPrevious);

    // Transition to Empty state as required by the spec: see
    // EmptyIgnoringPrevious's own definition for why.
    state_ = std::make_unique<InputStates::Empty>();
  } else if (auto* committing =
                 dynamic_cast<InputStates::Committing*>(currentPtr)) {
    handleCommittingState(context, prevPtr, committing);
  } else if (auto* inputting =
                 dynamic_cast<InputStates::Inputting*>(currentPtr)) {
    handleInputtingState(context, prevPtr, inputting);
  } else if (auto* candidates =
                 dynamic_cast<InputStates::ChoosingCandidate*>(currentPtr)) {
    handleCandidatesState(context, prevPtr, candidates);
  } else if (auto* selecting =
                 dynamic_cast<InputStates::SelectingDictionary*>(currentPtr)) {
    handleCandidatesState(context, prevPtr, selecting);
  } else if (auto* showingCharInfo =
                 dynamic_cast<InputStates::ShowingCharInfo*>(currentPtr)) {
    handleCandidatesState(context, prevPtr, showingCharInfo);
  } else if (auto* marking = dynamic_cast<InputStates::Marking*>(currentPtr)) {
    handleMarkingState(context, prevPtr, marking);
  } else if (auto* associatePhrases =
                 dynamic_cast<InputStates::AssociatedPhrases*>(currentPtr)) {
    handleCandidatesState(context, prevPtr, associatePhrases);
  } else if (auto* associatePhrasePlain =
                 dynamic_cast<InputStates::AssociatedPhrasesPlain*>(
                     currentPtr)) {
    handleCandidatesState(context, prevPtr, associatePhrasePlain);
  } else if (auto* numberInput =
                 dynamic_cast<InputStates::NumberInput*>(currentPtr)) {
    handleCandidatesState(context, prevPtr, numberInput);
  } else if (auto* icuTransformInput =
                 dynamic_cast<InputStates::IcuTransformInput*>(currentPtr)) {
    handleCandidatesState(context, prevPtr, icuTransformInput);
  } else if (auto* textTemplate =
                 dynamic_cast<InputStates::TextTemplate*>(currentPtr)) {
    handleCandidatesState(context, prevPtr, textTemplate);
  } else if (auto* selectingFeature =
               dynamic_cast<InputStates::SelectingFeature*>(currentPtr)) {
    handleCandidatesState(context, prevPtr, selectingFeature);
  } else if (auto* selectingDateMacro =
                 dynamic_cast<InputStates::SelectingDateMacro*>(currentPtr)) {
    handleCandidatesState(context, prevPtr, selectingDateMacro);
  } else if (auto* big5 = dynamic_cast<InputStates::Big5*>(currentPtr)) {
    handleStateWithCustomInput(context, big5->composingBuffer());  
  } else if (auto* customMenu =
                 dynamic_cast<InputStates::CustomMenu*>(currentPtr)) {
    handleCandidatesState(context, prevPtr, customMenu);
  }
}

void McBopomofoEngine::handleStateOrSequence(
    fcitx::InputContext* context, std::unique_ptr<InputState> newState) {
  if (auto* stateSeq =
          dynamic_cast<InputStates::StateSequence*>(newState.get())) {
    for (auto& s : stateSeq->states) {
      enterNewState(context, std::move(s));
    }
  } else {
    enterNewState(context, std::move(newState));
  }
}

void McBopomofoEngine::handleEmptyState(fcitx::InputContext* context,
                                        InputState* prev,
                                        InputStates::Empty* /*unused*/) {
  context->inputPanel().reset();
  context->updateUserInterface(fcitx::UserInterfaceComponent::InputPanel);
  if (auto* notEmpty = dynamic_cast<InputStates::NotEmpty*>(prev)) {
    context->commitString(notEmpty->composingBuffer);
  }
  context->updatePreedit();
}

void McBopomofoEngine::handleEmptyIgnoringPreviousState(
    fcitx::InputContext* context, InputState* /*unused*/,
    InputStates::EmptyIgnoringPrevious* /*unused*/) {
  context->inputPanel().reset();
  context->updateUserInterface(fcitx::UserInterfaceComponent::InputPanel);
  context->updatePreedit();
}

void McBopomofoEngine::handleCommittingState(fcitx::InputContext* context,
                                             InputState* /*prev*/,
                                             InputStates::Committing* current) {
  context->inputPanel().reset();
  context->updateUserInterface(fcitx::UserInterfaceComponent::InputPanel);
  if (!current->text.empty()) {
    context->commitString(current->text);
  }
  context->updatePreedit();
}

void McBopomofoEngine::handleInputtingState(fcitx::InputContext* context,
                                            InputState* /*unused*/,
                                            InputStates::Inputting* current) {
  context->inputPanel().reset();
  context->updateUserInterface(fcitx::UserInterfaceComponent::InputPanel);
  updatePreedit(context, current);
}

void McBopomofoEngine::handleCandidatesState(fcitx::InputContext* context,
                                             InputState* /*unused*/,
                                             InputState* current) {
  std::unique_ptr<fcitx::CommonCandidateList> candidateList =
      std::make_unique<fcitx::CommonCandidateList>();

  auto keysConfig = config_.selectionKeys.value();
  selectionKeys_.clear();

  InputStates::AssociatedPhrases* associatedPhrases =
      dynamic_cast<InputStates::AssociatedPhrases*>(state_.get());
  InputStates::AssociatedPhrasesPlain* associatedPhrasesPlain =
      dynamic_cast<InputStates::AssociatedPhrasesPlain*>(state_.get());
  InputStates::NumberInput* numberInput =
      dynamic_cast<InputStates::NumberInput*>(state_.get());
  InputStates::IcuTransformInput* icuTransformInput =
      dynamic_cast<InputStates::IcuTransformInput*>(state_.get());
  auto* textTemplate = dynamic_cast<InputStates::TextTemplate*>(current);

  if (auto* symbolTable =
          dynamic_cast<InputStates::ChoosingPunctuationList*>(current)) {
    // The symbol table is a grid of symbols, listed a row at a time.
    if (keysConfig == SelectionKeys::Key_asdfghjkl) {
      selectionKeys_ = fcitx::Key::keyListFromString("a s d f g h j k l");
      numpadSelectionKeys_ = fcitx::KeyList();
    } else if (keysConfig == SelectionKeys::Key_asdfzxcvb) {
      selectionKeys_ = fcitx::Key::keyListFromString("a s d f z x c v b");
      numpadSelectionKeys_ = fcitx::KeyList();
    } else {
      selectionKeys_ = fcitx::Key::keyListFromString("1 2 3 4 5 6 7 8 9");
      numpadSelectionKeys_ = fcitx::Key::keyListFromString(
          "KP_1 KP_2 KP_3 KP_4 KP_5 KP_6 KP_7 KP_8 KP_9");
    }
    std::vector<SmartLearning::SymbolSection> sections;
    if (!symbolTable->candidates.empty()) {
      const std::string& reading = symbolTable->candidates.front().reading;
      auto lm = languageModelLoader_->getLM();
      for (auto& [title, count] :
           lm->getSymbolSections(reading, _("Symbols for the Key"))) {
        sections.push_back({std::move(title), count});
      }
    }
    // The panel width is not known, so a row has as many symbols as there are
    // selection keys, each of them labeled.
    symbolGrid_ = std::make_unique<SmartLearning::SymbolGrid>(
        sections, symbolTable->candidates.size(), selectionKeys_.size(),
        selectionKeys_.size());
    if (!symbolTable->candidates.empty()) {
      // Start at the symbol of the punctuation key.
      symbolGrid_->setHighlighted(
          languageModelLoader_->getLM()->getSymbolInitialIndex(
              symbolTable->candidates.front().reading));
    }
    showSymbolTable(context);
    return;
  }

  bool useShiftKey = numberInput != nullptr || icuTransformInput != nullptr ||
                     associatedPhrasesPlain != nullptr;

  if (textTemplate != nullptr) {
    // Picked with Shift and a selection key, since the keys type the code.
    std::string keys = selectionKeyCharacters();
    std::vector<std::string> labels;
    for (char k : keys) {
      labels.push_back("⇧" + std::string(1, k) + ". ");
    }
    candidateList->setLabels(labels);
    candidateList->setPageSize(static_cast<int>(keys.size()));
  } else if ((associatedPhrases != nullptr &&
              associatedPhrases->autoTriggered)) {
    selectionKeys_ = fcitx::Key::keyListFromString("Shift+Return");
    std::vector<std::string> labels = {"⇧⏎ "};
    candidateList->setLabels(labels);
    candidateList->setPageSize(1);
  } else if (useShiftKey) {
    // This is for label appearance only. Shift+[1-9] keys can only be checked
    // via a raw key's key code, but Keys constructed with "Shift-" names does
    // not carry proper key codes.
    selectionKeys_ = fcitx::Key::keyListFromString(
        "Shift+1 Shift+2 Shift+3 Shift+4 Shift+5 Shift+6 Shift+7 Shift+8 "
        "Shift+9");
    std::vector<std::string> labels = {
        "⇧1. ", "⇧2. ", "⇧3. ", "⇧4. ", "⇧5. ", "⇧6. ", "⇧7. ", "⇧8. ", "⇧9. ",
    };
    candidateList->setLabels(labels);
    candidateList->setPageSize(static_cast<int>(selectionKeys_.size()));
  } else {
    if (keysConfig == SelectionKeys::Key_asdfghjkl) {
      selectionKeys_ = fcitx::Key::keyListFromString("a s d f g h j k l");
      numpadSelectionKeys_ = fcitx::KeyList();
    } else if (keysConfig == SelectionKeys::Key_asdfzxcvb) {
      selectionKeys_ = fcitx::Key::keyListFromString("a s d f z x c v b");
      numpadSelectionKeys_ = fcitx::KeyList();
    } else {
      selectionKeys_ = fcitx::Key::keyListFromString("1 2 3 4 5 6 7 8 9");
      numpadSelectionKeys_ = fcitx::KeyList();
      numpadSelectionKeys_.emplace_back(FcitxKey_KP_1);
      numpadSelectionKeys_.emplace_back(FcitxKey_KP_2);
      numpadSelectionKeys_.emplace_back(FcitxKey_KP_3);
      numpadSelectionKeys_.emplace_back(FcitxKey_KP_4);
      numpadSelectionKeys_.emplace_back(FcitxKey_KP_5);
      numpadSelectionKeys_.emplace_back(FcitxKey_KP_6);
      numpadSelectionKeys_.emplace_back(FcitxKey_KP_7);
      numpadSelectionKeys_.emplace_back(FcitxKey_KP_8);
      numpadSelectionKeys_.emplace_back(FcitxKey_KP_9);
    }
    candidateList->setSelectionKey(selectionKeys_);
    candidateList->setPageSize(static_cast<int>(selectionKeys_.size()));
  }

  fcitx::CandidateLayoutHint layoutHint = getCandidateLayoutHint();
  candidateList->setLayoutHint(layoutHint);

  KeyHandler::StateCallback callback =
      [this, context](std::unique_ptr<InputState> next) {
        handleStateOrSequence(context, std::move(next));
      };

  auto* choosing = dynamic_cast<InputStates::ChoosingCandidate*>(current);
  auto* selectingDictionary =
      dynamic_cast<InputStates::SelectingDictionary*>(current);
  auto* showingCharInfo = dynamic_cast<InputStates::ShowingCharInfo*>(current);
  auto* selectingFeature =
      dynamic_cast<InputStates::SelectingFeature*>(current);
  auto* selectingDateMacro =
      dynamic_cast<InputStates::SelectingDateMacro*>(current);
  auto* customMenu = dynamic_cast<InputStates::CustomMenu*>(current);

  if (choosing != nullptr) {
    // Construct the candidate list with special care for candidates that have
    // the same values. The display text of such a candidate will be in the
    // form of "value (reading)" to help user disambiguate those candidates.

    std::unordered_map<std::string, size_t> valueCountMap;

    for (const auto& c : choosing->candidates) {
      ++valueCountMap[c.value];
    }

    for (const auto& c : choosing->candidates) {
      // A symbol may be in more than one category of the symbol table, and
      // its reading is not shown.
      std::string displayText = SmartLearning::CandidateDisplayText(
          c.value, c.reading, valueCountMap[c.value]);

      std::unique_ptr<fcitx::CandidateWord> candidate =
          std::make_unique<McBopomofoCandidateWord>(fcitx::Text(displayText), c,
                                                    choosing->originalCursor,
                                                    keyHandler_, callback);
      candidateList->append(std::move(candidate));
    }
  } else if (selectingDictionary != nullptr) {
    size_t index = 0;
    for (const auto& menuItem : selectingDictionary->menu) {
      std::string displayText = menuItem;

      std::unique_ptr<fcitx::CandidateWord> candidate =
          std::make_unique<McBopomofoDictionaryServiceWord>(
              fcitx::Text(displayText), index, selectingDictionary, keyHandler_,
              callback);
      candidateList->append(std::move(candidate));
      index++;
    }
  } else if (showingCharInfo != nullptr) {
    std::vector<std::string> menu;
    menu.emplace_back(fmt::format(FmtRuntime(_("UTF8 String Length: {0}")),
                                  showingCharInfo->selectedPhrase.length()));
    size_t count = CodePointCount(showingCharInfo->selectedPhrase);
    menu.emplace_back(
        fmt::format(FmtRuntime(_("Code Point Count: {0}")), count));

    for (const auto& menuItem : menu) {
      std::string displayText = menuItem;

      std::unique_ptr<fcitx::CandidateWord> candidate =
          std::make_unique<McBopomofoTextOnlyCandidateWord>(
              fcitx::Text(displayText));
      candidateList->append(std::move(candidate));
    }
  } else if (associatedPhrases != nullptr) {
    std::vector<InputStates::ChoosingCandidate::Candidate> candidates =
        associatedPhrases->candidates;

    if (associatedPhrases->autoTriggered) {
      // If autoTriggered, show only the first candidate, and select it
      // immediately when user press Enter/Return.
      if (!candidates.empty()) {
        const auto& c = candidates.front();
        std::unique_ptr<fcitx::CandidateWord> candidate =
            std::make_unique<McBopomofoAssociatedPhraseCandidateWord>(
                fcitx::Text(c.value), c, associatedPhrases->prefixReading,
                associatedPhrases->prefixValue,
                associatedPhrases->prefixCursorIndex, keyHandler_, callback);
        candidateList->append(std::move(candidate));
      }
    } else {
      for (const auto& c : candidates) {
        std::unique_ptr<fcitx::CandidateWord> candidate =
            std::make_unique<McBopomofoAssociatedPhraseCandidateWord>(
                fcitx::Text(c.value), c, associatedPhrases->prefixReading,
                associatedPhrases->prefixValue,
                associatedPhrases->prefixCursorIndex, keyHandler_, callback);
        candidateList->append(std::move(candidate));
      }
    }
  } else if (associatedPhrasesPlain != nullptr) {
    std::vector<InputStates::ChoosingCandidate::Candidate> candidates =
        associatedPhrasesPlain->candidates;
    for (const auto& c : candidates) {
      std::unique_ptr<fcitx::CandidateWord> candidate =
          std::make_unique<McBopomofoCandidateWord>(fcitx::Text(c.value), c, 0,
                                                    keyHandler_, callback);
      candidateList->append(std::move(candidate));
    }
  } else if (selectingFeature != nullptr) {
    size_t index = 0;
    for (const auto& menuItem : selectingFeature->features) {
      std::string displayText = menuItem.name;

      std::unique_ptr<fcitx::CandidateWord> candidate =
          std::make_unique<McBopomofoFeatureWord>(fcitx::Text(displayText),
                                                  index, selectingFeature,
                                                  keyHandler_, callback);
      candidateList->append(std::move(candidate));
      index++;
    }
  } else if (selectingDateMacro != nullptr) {
    for (const auto& displayText : selectingDateMacro->menu) {
      std::unique_ptr<fcitx::CandidateWord> candidate =
          std::make_unique<McBopomofoDirectInsertWord>(fcitx::Text(displayText),
                                                       displayText, callback);
      candidateList->append(std::move(candidate));
    }
  } else if (numberInput != nullptr) {
    for (const auto& displayText : numberInput->candidates) {
      std::unique_ptr<fcitx::CandidateWord> candidate =
          std::make_unique<McBopomofoDirectInsertWord>(fcitx::Text(displayText),
                                                       displayText, callback);
      candidateList->append(std::move(candidate));
    }
  } else if (icuTransformInput != nullptr) {
    for (const auto& displayText : icuTransformInput->candidates) {
      std::unique_ptr<fcitx::CandidateWord> candidate =
          std::make_unique<McBopomofoDirectInsertWord>(fcitx::Text(displayText),
                                                       displayText, callback);
      candidateList->append(std::move(candidate));
    }
  } else if (textTemplate != nullptr) {
    for (size_t i = 0; i < textTemplate->texts.size(); ++i) {
      candidateList->append(std::make_unique<McBopomofoDirectInsertWord>(
          fcitx::Text(textTemplate->displayTexts[i]), textTemplate->texts[i],
          callback));
    }
  } else if (customMenu != nullptr) {
    size_t index = 0;
    for (const auto& entry : customMenu->entries) {
      std::string displayText = entry.name;
      std::unique_ptr<fcitx::CandidateWord> candidate =
          std::make_unique<McBopomofoCustomMenuWord>(fcitx::Text(displayText),
                                                     index, customMenu);
      candidateList->append(std::move(candidate));
      index++;
    }
  }

  candidateList->toCursorMovable()->nextCandidate();
  context->inputPanel().reset();
  context->inputPanel().setCandidateList(std::move(candidateList));
  context->updateUserInterface(fcitx::UserInterfaceComponent::InputPanel);

  auto* notEmpty = dynamic_cast<InputStates::NotEmpty*>(current);
  if (notEmpty != nullptr) {
    updatePreedit(context, notEmpty);
  }
}

void McBopomofoEngine::handleMarkingState(fcitx::InputContext* context,
                                          InputState* /*unused*/,
                                          InputStates::Marking* current) {
  context->inputPanel().reset();
  context->updateUserInterface(fcitx::UserInterfaceComponent::InputPanel);
  updatePreedit(context, current);
}

void McBopomofoEngine::handleStateWithCustomInput(fcitx::InputContext* context,
                                                  std::string composingBuffer) {
  context->inputPanel().reset();
  context->updateUserInterface(fcitx::UserInterfaceComponent::InputPanel);

  bool useClientPreedit = this->useClientPreedit(context);
  fcitx::TextFormatFlags normalFormat{useClientPreedit
                                          ? fcitx::TextFormatFlag::Underline
                                          : fcitx::TextFormatFlag::NoFlag};
  fcitx::Text preedit;
  preedit.append(composingBuffer, normalFormat);
  preedit.setCursor(static_cast<int>(composingBuffer.length()));

  if (useClientPreedit) {
    context->inputPanel().setClientPreedit(preedit);
  } else {
    context->inputPanel().setPreedit(preedit);
  }
  context->updatePreedit();
}

fcitx::CandidateLayoutHint McBopomofoEngine::getCandidateLayoutHint() const {
  fcitx::CandidateLayoutHint layoutHint = fcitx::CandidateLayoutHint::NotSet;

  if (dynamic_cast<InputStates::NumberInput*>(state_.get()) != nullptr ||
      dynamic_cast<InputStates::IcuTransformInput*>(state_.get()) != nullptr ||
      dynamic_cast<InputStates::TextTemplate*>(state_.get()) != nullptr ||
      dynamic_cast<InputStates::SelectingDictionary*>(state_.get()) !=
          nullptr ||
      dynamic_cast<InputStates::ShowingCharInfo*>(state_.get()) != nullptr ||
      dynamic_cast<InputStates::SelectingFeature*>(state_.get()) != nullptr ||
      dynamic_cast<InputStates::SelectingDateMacro*>(state_.get()) != nullptr) {
    return fcitx::CandidateLayoutHint::Vertical;
  }

  auto* choosingCandidate =
      dynamic_cast<InputStates::ChoosingCandidate*>(state_.get());
  if (choosingCandidate != nullptr) {
    auto candidates = choosingCandidate->candidates;
    for (const InputStates::ChoosingCandidate::Candidate& candidate :
         candidates) {
      std::string value = candidate.value;
      if (McBopomofo::CodePointCount(value) >
          kForceVerticalCandidateThreshold) {
        return fcitx::CandidateLayoutHint::Vertical;
      }
    }
  }

  switch (config_.candidateLayout.value()) {
    case McBopomofo::CandidateLayoutHint::Vertical:
      layoutHint = fcitx::CandidateLayoutHint::Vertical;
      break;
    case McBopomofo::CandidateLayoutHint::Horizontal:
      layoutHint = fcitx::CandidateLayoutHint::Horizontal;
      break;
    case McBopomofo::CandidateLayoutHint::NotSet:
      layoutHint = fcitx::CandidateLayoutHint::NotSet;
      break;
  }

  return layoutHint;
}

void McBopomofoEngine::updatePreedit(fcitx::InputContext* context,
                                     InputStates::NotEmpty* state) {
  bool useClientPreedit = this->useClientPreedit(context);
  fcitx::TextFormatFlags normalFormat{useClientPreedit
                                          ? fcitx::TextFormatFlag::Underline
                                          : fcitx::TextFormatFlag::NoFlag};
  fcitx::Text preedit;
  if (auto* marking = dynamic_cast<InputStates::Marking*>(state)) {
    preedit.append(marking->head, normalFormat);
    preedit.append(marking->markedText, fcitx::TextFormatFlag::HighLight);
    preedit.append(marking->tail, normalFormat);
  } else {
    // Note: dynamic_cast<InputStates::Marking*> in enterNewState() ensures
    // state is not nullptr
    // NOLINTNEXTLINE(clang-analyzer-core.NonNullParamChecker)
    preedit.append(state->composingBuffer, normalFormat);
  }
  preedit.setCursor(static_cast<int>(state->cursorIndex));

  if (useClientPreedit) {
    context->inputPanel().setClientPreedit(preedit);
  } else {
    context->inputPanel().setPreedit(preedit);
  }

  context->inputPanel().setAuxDown(fcitx::Text(state->tooltip));
  context->updatePreedit();
}

void McBopomofoEngine::showAndClearUserFileIssues() {
  size_t numIssues = 0;
  const size_t MAX_ISSUES = 3;

  std::stringstream sst;
  for (const auto& issue : userFileIssues_) {
    switch (issue.fileType) {
      case McBopomofoLM::UserFileType::USER_PHRASES:
        sst << _("User Phrases");
        break;
      case McBopomofoLM::UserFileType::EXCLUDED_PHRASES:
        sst << _("Excluded Phrases");
        break;
      case McBopomofoLM::UserFileType::PHRASE_REPLACEMENT_MAP:
        sst << _("Phrase Replacement File");
        break;
      default:
        // should not happen
        break;
    }

    sst << " (";
    sst << issue.path.string();
    sst << ") ";
    sst << fmt::format(FmtRuntime(_("line {0}")), issue.lineNumber);
    sst << ": ";

    switch (issue.issueType) {
      case McBopomofoLM::IssueType::NO_ISSUE:
        // Ignored.
        break;
      case McBopomofoLM::IssueType::MISSING_SECOND_COLUMN:
        sst << _("Only one column was found.");
        break;
      case McBopomofoLM::IssueType::NULL_CHARACTER_IN_TEXT:
        sst << _("Illegal NULL character was found.");
        break;
      default:
        // should not happen
        break;
    }
    sst << "\n";

    ++numIssues;
    if (numIssues >= MAX_ISSUES) {
      break;
    }
  }

  if (userFileIssues_.size() > MAX_ISSUES) {
    sst << "\n";
    size_t remainingIssues = userFileIssues_.size() - MAX_ISSUES;

    if (remainingIssues == 1) {
      sst << _("And one more issue.");
    } else {
      sst << fmt::format(FmtRuntime(_("And {0} more issues.")),
                         remainingIssues);
    }
    sst << "\n";
  }

  if (notifications()) {
    notifications()->call<fcitx::INotifications::showTip>(
        "mcbopomofo-user-file-issues", _("McBopomofo"), "fcitx_mcbopomofo",
        _("Issues found in user files"), sst.str(),
        kFcitx5NotificationTimeoutInMs);
  } else {
    FCITX_MCBOPOMOFO_ERROR() << sst.str();
  }
  userFileIssues_.clear();
}

FCITX_ADDON_FACTORY(McBopomofoEngineFactory);

}  // namespace McBopomofo
