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

#ifndef SRC_KEYHANDLER_H_
#define SRC_KEYHANDLER_H_

#include <functional>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "DictionaryService.h"
#include "EnglishWords.h"
#include "Engine/Mandarin/Mandarin.h"
#include "Engine/UserOverrideModel.h"
#include "Engine/gramambular2/language_model.h"
#include "Engine/gramambular2/reading_grid.h"
#include "InputMode.h"
#include "InputState.h"
#include "Key.h"
#include "LanguageModelLoader.h"
#include "LearnPhrase.h"
#include "TemporaryPhrases.h"
#include "UserOverrideStore.h"

namespace McBopomofo {

enum class KeyHandlerCtrlEnter {
  Disabled,
  OutputBpmfReadings,
  OutputHTMLRubyText,
  OutputHanyuPinyin,
  OutputTaiwanBrailleUnicode,
  OutputTaiwanBrailleAscii,
};

enum class BrailleType;

class KeyHandler {
 public:
  class LocalizedStrings;

  explicit KeyHandler(
      std::shared_ptr<Formosa::Gramambular2::LanguageModel> languageModel,
      std::shared_ptr<VariantAnnotator> variantAnnotator,
      std::shared_ptr<UserPhraseAdder> userPhraseAdder,
      std::unique_ptr<LocalizedStrings> localizedStrings,
      std::shared_ptr<SmartLearning::UserOverrideStore> userOverrideStore);

  using StateCallback =
      std::function<void(std::unique_ptr<McBopomofo::InputState>)>;
  using ErrorCallback = std::function<void(void)>;
  using SelectCurrentCandidateCallback = std::function<void(void)>;

  // Given a fcitx5 KeyEvent and the current state, invokes the stateCallback if
  // a new state is entered, or errorCallback will be invoked. Returns true if
  // the key should be absorbed, signaling that the key is accepted and handled,
  // or false if the event should be let pass through.
  bool handle(Key key, McBopomofo::InputState* state,
              StateCallback stateCallback, ErrorCallback errorCallback);

  bool handleAssociatedPhrases(InputStates::Inputting* state,
                               StateCallback stateCallback,
                               ErrorCallback errorCallback,
                               bool autoTriggered = false);

  void handleForceCommitAndReset(StateCallback stateCallback);

  bool handleNumberInput(Key key, InputStates::NumberInput* state,
                         StateCallback stateCallback,
                         KeyHandler::ErrorCallback errorCallback);

  bool handleIcuTransformInput(
      Key key, McBopomofo::InputStates::IcuTransformInput* state,
      StateCallback stateCallback, KeyHandler::ErrorCallback errorCallback);

  // Candidate selected. Can assume the context is in a candidate state.
  void candidateSelected(
      const InputStates::ChoosingCandidate::Candidate& candidate,
      size_t originalCursor, StateCallback stateCallback);

  void candidateAssociatedPhraseSelected(
      size_t index, const InputStates::ChoosingCandidate::Candidate& candidate,
      const std::string& selectedReading, const std::string& selectedValue,
      const StateCallback& stateCallback);

  void dictionaryServiceSelected(std::string phrase, size_t index,
                                 InputState* currentState,
                                 StateCallback stateCallback);

  bool candidatePanelPunctuationMaybeEntered(Key key, size_t originalCursor,
                                             StateCallback stateCallback);

  /// Candidate panel for punctuation list canceled. Can assume the context is
  /// in a ChoosingPunctuationList state.
  void candidatePanelPunctuationListCancelled(size_t originalCursor,
                                              StateCallback stateCallback);

  // Candidate panel canceled. Can assume the context is in a candidate
  // state.
  void candidatePanelCancelled(size_t originalCursor,
                               StateCallback stateCallback);

  // Workaround for the Traditional Bopomofo mode.
  bool handleCandidateKeyForTraditionalBopomofoIfRequired(
      Key key, SelectCurrentCandidateCallback SelectCurrentCandidateCallback,
      StateCallback stateCallback, ErrorCallback errorCallback);

  void boostPhrase(const std::string& reading, const std::string& value);

  void excludePhrase(const std::string& reading, const std::string& value);

  void reset();

#pragma region Dictionary Services

  bool hasDictionaryServices();

  std::unique_ptr<InputStates::SelectingDictionary>
  buildSelectingDictionaryState(
      std::unique_ptr<InputStates::NotEmpty> nonEmptyState,
      const std::string& selectedPhrase, size_t selectedIndex);

#pragma endregion Dictionary Services

#pragma region Settings

  McBopomofo::InputMode inputMode();

  // Sets the input mode.
  void setInputMode(McBopomofo::InputMode mode);

  // Sets the Bopomofo keyboard layout.
  void setKeyboardLayout(
      const Formosa::Mandarin::BopomofoKeyboardLayout* layout);

  // Sets if we should select phrase after cursor as candidate.
  void setSelectPhraseAfterCursorAsCandidate(bool flag);

  // Sets if we should move cursor after selection.
  void setMoveCursorAfterSelection(bool flag);

  // Sets if we should put lowercased letters into the composing buffer.
  void setPutLowercaseLettersToComposingBuffer(bool flag);

  // Sets if the ESC key clears entire composing buffer.
  void setEscKeyClearsEntireComposingBuffer(bool flag);

  // Sets if the ESC key removes only the mistyped text before the cursor, and
  // pressing it twice clears everything. When on, it replaces the option
  // above.
  void setEscTrimsMistypedText(bool flag);

  // A key handled outside of handle() (the candidate window) breaks a double
  // press of the ESC key.
  void breakEscDoublePress() { lastKeyWasTrimmingEsc_ = false; }

  // Keeps an invalid reading editable and clears its old tone when a new
  // Bopomofo component is entered.
  void setKeepInvalidSyllableForFurtherInput(bool flag);

  // Sets if the Shift + Enter key is enabled.
  void setShiftEnterEnabled(bool flag);

  // Sets the behaviour of the Ctrl + Enter key
  void setCtrlEnterKeyBehavior(KeyHandlerCtrlEnter behavior);

  // Sets if associated phrases is enabled or not.
  void setAssociatedPhrasesEnabled(bool enabled);

  // Sets if half width punctuation is enabled or not.
  void setHalfWidthPunctuationEnabled(bool enabled);

  // Sets the lambda for adding the phrases.
  // Invoked after each choice the user made has been recorded into the
  // learned-choice store, so that the owner can schedule saving it.
  void setOnUserOverrideObserved(std::function<void()> callback) {
    onUserOverrideObserved_ = std::move(callback);
  }

  void setOnAddNewPhrase(
      std::function<void(const std::string&)> onAddNewPhrase);

  // Sets whether English can be typed without Shift or Caps Lock: letters
  // that cannot be Bopomofo, or are typed after an uppercase letter, are put
  // as is until a space, a symbol or Backspace.
  void setSmartChineseEnglishSwitchEnabled(bool enabled);

  // Sets whether a space is put between Chinese and English typed next to
  // each other.
  void setAutoSpaceBetweenChineseAndEnglish(bool enabled);

  // Sets whether punctuation typed right after an English letter or a digit
  // is half width, like "hello," and "3.5".
  void setHalfWidthPunctuationAfterEnglish(bool enabled);

  // Sets whether the case of an English word is fixed upon a space, like
  // "iphone" to "iPhone".
  void setFixEnglishWordCase(bool enabled);

  // The English words typed, to tell words from mistyped Bopomofo and to fix
  // their case. Words committed are recorded in it.
  void setEnglishWordStore(
      std::shared_ptr<SmartLearning::EnglishWordStore> store);

  // Invoked when the smart English mode is entered or left.
  void setOnSmartEnglishModeChanged(std::function<void(bool)> callback) {
    onSmartEnglishModeChanged_ = std::move(callback);
  }

  // Invoked after English words have been recorded, so that the owner can
  // schedule saving them.
  void setOnEnglishWordsRecorded(std::function<void()> callback) {
    onEnglishWordsRecorded_ = std::move(callback);
  }

  // The phrases picked once. A phrase picked becomes a temporary phrase, and
  // is learned as a user phrase when picked again or typed again and
  // committed. Without the store, no phrase is learned.
  void setTemporaryPhraseStore(
      std::shared_ptr<SmartLearning::TemporaryPhraseStore> store) {
    temporaryPhraseStore_ = std::move(store);
  }

  // Writes the phrases, each a value and a reading, to the user phrase file
  // at once, and returns the ones learned.
  using UserPhraseLearner =
      std::function<std::vector<SmartLearning::Phrase>(
          const std::vector<SmartLearning::Phrase>&)>;
  void setUserPhraseLearner(UserPhraseLearner learner) {
    userPhraseLearner_ = std::move(learner);
  }

  // Invoked after the temporary phrases changed, so that the owner can give
  // them to the language model and schedule saving them.
  void setOnPhraseLearned(std::function<void()> callback) {
    onPhraseLearned_ = std::move(callback);
  }

  // Whether the letters typed are being put as English.
  [[nodiscard]] bool isSmartEnglishMode() const { return smartEnglishMode_; }

  //  Sets whether repeated punctuation characters should be used to select
  //  candidates.
  void setRepeatedPunctuationToSelectCandidateEnabled(bool enabled);

  // Sets whether the space key should be used to choose candidates.
  void setChooseCandidateUsingSpace(bool enabled);

  // Sets whether to enable the Bopomofo variant annotator.
  void setBopomofoFontAnnotationSupportEnabled(bool enabled);

  bool bopomofoFontAnnotationSupportEnabled() const {
    return bopomofoFontAnnotationSupportEnabled_;
  }

  // Compute the actual candidate cursor index based on the current index.
  size_t actualCandidateCursorIndex();
  // Compute the actual candidate cursor index.
  size_t computeActualCandidateCursorIndex(size_t index);

  // Get the current cursor index.
  size_t candidateCursorIndex();
  // Set the current cursor index.
  void setCandidateCursorIndex(size_t newCursor);

  // Build an Inputting state.
  std::unique_ptr<InputStates::Inputting> buildInputtingState();

  // Build a Choosing Candidate state.
  std::unique_ptr<InputStates::ChoosingCandidate> buildChoosingCandidateState(
      InputStates::NotEmpty* nonEmptyState, size_t originalCursor);

  // Reading joiner for retrieving unigrams from the language model.
  static constexpr char kJoinSeparator[] = "-";

  // Build an Associated Phrase state. The prefixCursorIndex is where the
  // prefix node is actually located in the grid.
  std::unique_ptr<InputStates::AssociatedPhrases> buildAssociatedPhrasesState(
      std::unique_ptr<InputStates::NotEmpty> previousState,
      size_t prefixCursorIndex, std::string prefixCombinedReading,
      std::string prefixValue, size_t selectedCandidateIndex, bool useShiftKey);

  // Build an Associated Phrase state. The candidateCursorIndex is where the
  // user-visible cursor was *before* the ChoosingCandidateState was entered,
  // which actually means the place where the candidate were collected. You
  // must call this method if you want to enter the Associated Phrase, since
  // the candidate cursor position is not always the prefix node position
  // (consider the Hanyin cursor mode)!
  std::unique_ptr<InputStates::AssociatedPhrases>
  buildAssociatedPhrasesStateFromCandidateChoosingState(
      std::unique_ptr<InputStates::NotEmpty> previousState,
      size_t candidateCursorIndex, std::string prefixCombinedReading,
      std::string prefixValue, size_t selectedCandidateIndex);

  // Build an Associated Phrases Plain state by the given key. It could be
  // nullptr when there is no associated phrases.
  std::unique_ptr<InputStates::AssociatedPhrasesPlain>
  buildAssociatedPhrasesPlainState(const std::string& reading,
                                   const std::string& value);

#pragma endregion Settings

 private:
  void clearReading();

  bool handleBig5(Key key, McBopomofo::InputStates::Big5* state,
                  StateCallback stateCallback,
                  KeyHandler::ErrorCallback errorCallback);

  bool handleTabKey(bool isShiftPressed, McBopomofo::InputState* state,
                    const StateCallback& stateCallback,
                    const ErrorCallback& errorCallback);
  bool handleCursorKeys(Key key, McBopomofo::InputState* state,
                        const StateCallback& stateCallback,
                        const ErrorCallback& errorCallback);
  bool handleDeleteKeys(Key key, McBopomofo::InputState* state,
                        const StateCallback& stateCallback,
                        const ErrorCallback& errorCallback);
  bool handlePunctuation(const std::string& punctuationUnigramKey,
                         McBopomofo::InputState* state,
                         const StateCallback& stateCallback,
                         const ErrorCallback& errorCallback);

  // Get the head and the tail of current composed string, separated by the
  // current cursor.
  struct ComposedString {
    std::string head;
    std::string tail;
    // Any tooltip during the build process.
    std::string tooltip;
  };
  ComposedString getComposedString(size_t builderCursor);
  std::string getHTMLRubyText();
  std::string getHanyuPinyin();
  std::string getTaiwanBraille(BrailleType type);

  // Build a Marking state, ranging from beginCursorIndex to the current
  // builder cursor. It doesn't matter if the beginCursorIndex is behind or
  // after the builder cursor.
  std::unique_ptr<InputStates::Marking> buildMarkingState(
      size_t beginCursorIndex);

  // Pin a node with a fixed unigram value, usually a candidate.
  void pinNode(size_t originalCursor,
               const InputStates::ChoosingCandidate::Candidate& candidate,
               bool useMoveCursorAfterSelectionSetting = true);

  // Pin a node with an associated phrase; an associated phrase has a prefix
  // that is either the current node at the cursor, or a new "override" phrase
  // from the choosing-candidate state; the actual associated phrase will also
  // contain the prefix. This allows the following two scenarios:
  //
  // (1) the current walk is 得 and we want to pin the phrase 得到; in this
  //     case, the prefixReading is ㄉㄜˊ and prefixValue is 得, and the
  //     associated phrase's reading and value are ㄉㄜˊ-ㄉㄠˋ and 得到
  //     respectively.
  // (2) the current walk is 得 but we want to pin the phrase 德性, coming
  // from
  //     the choosing-candidate state; in this case, the prefix reading and
  //     value is now ㄉㄜˊ and 德, and the associated phrase is ㄉㄜˊ-ㄒㄧㄥˋ
  //     and 德性 respectively.
  void pinNodeWithAssociatedPhrase(size_t prefixCursorIndex,
                                   const std::string& prefixReading,
                                   const std::string& prefixValue,
                                   const std::string& associatedPhraseReading,
                                   const std::string& associatedPhraseValue);

  void walk();

#pragma region Smart_Chinese_English

  // Whether the key may switch between Chinese and English.
  bool isSmartSwitchingAvailable(const Key& key) const;
  void setSmartEnglishMode(bool englishMode);
  // Removes the mistyped text before the cursor (and the unfinished reading).
  // Returns false if there is nothing to remove.
  bool trimMistypedTextBeforeCursor();
  // The ESC key in the Inputting state when it trims mistyped text.
  void handleEscTrimmingMistypedText(bool pressedTwice,
                                     const StateCallback& stateCallback);
  // The keys typed for the current reading.
  std::string rawKeysOfCurrentReading() const;
  // Puts the text into the composing buffer as is, one reading for each
  // character, after a space if it starts with a letter right after Chinese.
  void insertLiteralText(const std::string& text);
  // Puts a space before English typed after Chinese (|beforeEnglish|), or
  // before Chinese typed after English. Returns true if a space was put.
  bool insertAutoSpaceIfNeeded(bool beforeEnglish);

  // Whether the cursor is right after an English letter or a half-width digit
  // with no bopomofo reading pending.
  bool isAfterHalfWidthAlphanumeric() const;
  // Turns the unfinished reading into English and starts the English mode
  // with the text.
  void switchToSmartEnglish(const std::string& text);
  // Fixes the case of the English word before the cursor, like "iPhone".
  void fixCaseOfEnglishWordBeforeCursor();
  // Turns the last letters before the cursor into Chinese when the key ends
  // a syllable typed right after an English word.
  bool convertTrailingEnglishToChinese(char key,
                                       const StateCallback& stateCallback);
  // Records the English words in the composition about to be committed.
  void learnEnglishWordsFromWalk();
  // Learns from the composition about to be committed: the temporary phrases
  // typed again become user phrases, and the English words are recorded.
  void learnFromCommittedComposition();
  // Learns the phrase of the candidate just picked at |cursor|, or the
  // characters that broke up the phrase there in |walkBefore|.
  void learnPickedPhrase(
      const Formosa::Gramambular2::ReadingGrid::NodePtr& node,
      const Formosa::Gramambular2::ReadingGrid::WalkResult& walkBefore,
      size_t cursor);
  bool isEnglishWord(const std::string& text) const;
  bool isEnglishWordOrPrefix(const std::string& text) const;

#pragma endregion Smart_Chinese_English

  std::shared_ptr<Formosa::Gramambular2::LanguageModel> lm_;
  std::shared_ptr<VariantAnnotator> variantAnnotator_;
  Formosa::Gramambular2::ReadingGrid grid_;
  std::shared_ptr<UserPhraseAdder> userPhraseAdder_;
  std::unique_ptr<LocalizedStrings> localizedStrings_;

  std::shared_ptr<SmartLearning::UserOverrideStore> userOverrideStore_;
  std::function<void()> onUserOverrideObserved_;
  Formosa::Mandarin::BopomofoReadingBuffer reading_;
  // The keys typed for the reading being composed, so that they can be
  // turned into English as typed.
  std::string rawKeysOfReading_;
  bool smartEnglishMode_ = false;
  bool escTrimsMistypedText_ = false;
  // The last key was an ESC that removed mistyped text or only showed the
  // hint, so another ESC clears everything.
  bool lastKeyWasTrimmingEsc_ = false;
  // Only digits have been typed in the English mode, so a space only leaves
  // the mode, like "2026年".
  bool smartEnglishDigitsOnly_ = false;
  std::shared_ptr<SmartLearning::EnglishWordStore> englishWordStore_;
  std::function<void(bool)> onSmartEnglishModeChanged_;
  std::function<void()> onEnglishWordsRecorded_;
  std::shared_ptr<SmartLearning::TemporaryPhraseStore> temporaryPhraseStore_;
  UserPhraseLearner userPhraseLearner_;
  std::function<void()> onPhraseLearned_;
  // The phrases that became temporary phrases in this composition; committing
  // them does not count as typing them again.
  std::set<SmartLearning::Phrase> phrasesPickedInComposition_;
  Formosa::Gramambular2::ReadingGrid::WalkResult latestWalk_;
  std::shared_ptr<DictionaryServices> dictionaryServices_;

#pragma region Settings

  McBopomofo::InputMode inputMode_ = McBopomofo::InputMode::McBopomofo;
  bool selectPhraseAfterCursorAsCandidate_ = false;
  bool moveCursorAfterSelection_ = false;
  bool putLowercaseLettersToComposingBuffer_ = false;
  bool escKeyClearsEntireComposingBuffer_ = false;
  bool keepInvalidSyllableForFurtherInput_ = false;
  bool readingCompositionFailed_ = false;
  bool shiftEnterEnabled_ = true;
  bool associatedPhrasesEnabled_ = false;
  bool halfWidthPunctuationEnabled_ = false;
  bool repeatedPunctuationToSelectCandidateEnabled_ = false;
  bool chooseCandidateUsingSpace_ = true;
  bool bopomofoFontAnnotationSupportEnabled_ = false;
  bool smartChineseEnglishSwitchEnabled_ = true;
  bool autoSpaceBetweenChineseAndEnglish_ = true;
  bool halfWidthPunctuationAfterEnglish_ = true;
  bool fixEnglishWordCase_ = true;
  KeyHandlerCtrlEnter ctrlEnterKey_ = KeyHandlerCtrlEnter::Disabled;
  std::function<void(const std::string&)> onAddNewPhrase_;

#pragma endregion Settings

 public:
  // Localization helper. We use dependency injection, that is, passing an
  // instance of the class when constructing KeyHandler, so that KeyHandler
  // itself is not concerned with how localization is implemented.
  class LocalizedStrings {
   public:
    virtual ~LocalizedStrings() = default;

    // Reference string: "Cursor is between syllables {0} and {1}"
    virtual std::string cursorIsBetweenSyllables(
        const std::string& prevReading, const std::string& nextReading) = 0;
    // Reference string: "{0} syllables required"
    virtual std::string syllablesRequired(size_t syllables) = 0;
    // Reference string: "{0} syllables maximum"
    virtual std::string syllablesMaximum(size_t syllables) = 0;
    // Reference string: "phrase already exists"
    virtual std::string phraseAlreadyExists() = 0;
    // Reference string: "press Enter to add the phrase"
    virtual std::string pressEnterToAddThePhrase() = 0;
    // Reference string: "Marked: {0}, syllables: {1}, {2}"
    virtual std::string markedWithSyllablesAndStatus(
        const std::string& marked, const std::string& readingUiText,
        const std::string& status) = 0;
    // This returns any of the four possible strings when Bopomofo Font
    // Annotation Mode is on. Reference strings:
    // - "Bopomofo annotation: variant selectors and PUA blocks in text"
    // - "Bopomofo annotation: variant selectors in text"
    // - "Bopomofo annotation: PUA blocks in text"
    // - "Bopomofo annotation support on"
    virtual std::string bopomofoFontAnnotationModeTooltip(
        bool hasUnicodeVariantSelectors, bool hasPUABlocks) = 0;
    // Reference string: "cannot add new phrases when Bopomofo annotation is
    // on"
    virtual std::string markingNotAvailableInFontAnnotationMode() = 0;
    // Reference string: "Press Esc again to clear all"
    virtual std::string pressEscAgainToClearAll() = 0;
  };
};

}  // namespace McBopomofo

#endif  // SRC_KEYHANDLER_H_
