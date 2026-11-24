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

#ifndef SRC_MCBOPOMOFO_H_
#define SRC_MCBOPOMOFO_H_

#include <fcitx-config/configuration.h>
#include <fcitx-config/enum.h>
#include <fcitx-config/iniparser.h>
#include <fcitx-utils/i18n.h>
#include <fcitx-utils/standardpath.h>
#include <fcitx/action.h>
#include <fcitx/addonfactory.h>
#include <fcitx/addonmanager.h>
#include <fcitx/candidatelist.h>
#include <fcitx/inputmethodengine.h>
#include <fcitx/instance.h>
#include <fcitx-utils/event.h>

#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

#include "InputState.h"
#include "KeyHandler.h"
#include "LanguageModelLoader.h"
#include "CommonSentences.h"
#include "RemovedEntries.h"
#include "SymbolTable.h"
#include "TemporaryPhrases.h"
#include "TextTemplates.h"
#include "UserOverrideStore.h"
#include "PathCompat.h"

namespace McBopomofo {

enum class BopomofoKeyboardLayout {
  Standard,
  Eten,
  Hsu,
  Et26,
  HanyuPinyin,
  IBM
};
FCITX_CONFIG_ENUM_NAME_WITH_I18N(BopomofoKeyboardLayout, N_("standard"),
                                 N_("eten"), N_("hsu"), N_("et26"),
                                 N_("hanyupinyin"), N_("ibm"));

// The key layout the key handler uses for each configured keyboard layout.
inline const Formosa::Mandarin::BopomofoKeyboardLayout* layoutForConfig(
    BopomofoKeyboardLayout layout) {
  switch (layout) {
    case BopomofoKeyboardLayout::Standard:
      return Formosa::Mandarin::BopomofoKeyboardLayout::StandardLayout();
    case BopomofoKeyboardLayout::Eten:
      return Formosa::Mandarin::BopomofoKeyboardLayout::ETenLayout();
    case BopomofoKeyboardLayout::Hsu:
      return Formosa::Mandarin::BopomofoKeyboardLayout::HsuLayout();
    case BopomofoKeyboardLayout::Et26:
      return Formosa::Mandarin::BopomofoKeyboardLayout::ETen26Layout();
    case BopomofoKeyboardLayout::HanyuPinyin:
      return Formosa::Mandarin::BopomofoKeyboardLayout::HanyuPinyinLayout();
    case BopomofoKeyboardLayout::IBM:
      return Formosa::Mandarin::BopomofoKeyboardLayout::IBMLayout();
  }
  return Formosa::Mandarin::BopomofoKeyboardLayout::StandardLayout();
}

// Whether the quick-switch menu item for |item| is checked while the
// configured layout is |configured|. Layouts other than the two quick items
// leave both unchecked.
inline bool isQuickLayoutChecked(BopomofoKeyboardLayout configured,
                                 BopomofoKeyboardLayout item) {
  return configured == item;
}

// The vocabulary manager, when it is built with the input method.
#ifdef MCBOPOMOFO_VOCABULARY_MANAGER_PATH
inline constexpr bool kHasVocabularyManager = true;
inline constexpr char kVocabularyManagerPath[] =
    MCBOPOMOFO_VOCABULARY_MANAGER_PATH;
#else
inline constexpr bool kHasVocabularyManager = false;
inline constexpr char kVocabularyManagerPath[] = "";
#endif

// The command that opens the vocabulary manager |program| on the user data
// folder and as the device of the input method. What is empty is left out,
// for the manager to read from the config file.
inline std::vector<std::string> vocabularyManagerCommand(
    const std::string& program, const std::string& userDataPath,
    const std::string& deviceID) {
  std::vector<std::string> command{program};
  if (!userDataPath.empty()) {
    command.insert(command.end(), {"--data-folder", userDataPath});
  }
  if (!deviceID.empty()) {
    command.insert(command.end(), {"--device-id", deviceID});
  }
  return command;
}

// The user guide, installed with the input method.
#ifdef MCBOPOMOFO_USER_GUIDE_PATH
inline constexpr char kUserGuidePath[] = MCBOPOMOFO_USER_GUIDE_PATH;
#else
inline constexpr char kUserGuidePath[] =
    "/usr/share/fcitx5-mcbopomofo/user-guide.html";
#endif

// The address that opens the user guide at |path|, at |section| (the id of a
// section, such as "about") if it is not empty.
inline std::string userGuideURL(const std::string& path,
                                const std::string& section = "") {
  std::string url = "file://" + path;
  if (!section.empty()) {
    url += "#" + section;
  }
  return url;
}

// The names the menu items are registered by.
inline constexpr char kEditUserPhrasesAction[] = "mcbopomofo-user-phrases-edit";
inline constexpr char kEditExcludedPhrasesAction[] =
    "mcbopomofo-user-excluded-phrases-edit";
inline constexpr char kManageVocabularyAction[] =
    "mcbopomofo-manage-vocabulary";
inline constexpr char kEditTextTemplatesAction[] =
    "mcbopomofo-text-templates-edit";
inline constexpr char kEditSymbolTableAction[] = "mcbopomofo-symbol-table-edit";
inline constexpr char kFloatingCompositionAction[] =
    "mcbopomofo-floating-composition";
inline constexpr char kUserGuideAction[] = "mcbopomofo-user-guide";
inline constexpr char kAboutAction[] = "mcbopomofo-about";

// The menu items after the input options, in menu order, in |mode|. The user
// phrase files of Plain Bopomofo are not edited from the menu; the text
// templates and the symbol table are the same files in both modes. The text
// templates are edited in the vocabulary manager when there is one, so the
// menu item that opens their file is only for a build without it. The
// floating composition item is listed, but is shown only for a program with a
// name.
inline std::vector<std::string> userFileAndHelpMenuActions(
    InputMode mode, bool hasVocabularyManager) {
  std::vector<std::string> names;
  if (mode == InputMode::McBopomofo) {
    names.insert(names.end(),
                 {kEditUserPhrasesAction, kEditExcludedPhrasesAction});
  }
  if (hasVocabularyManager) {
    names.emplace_back(kManageVocabularyAction);
  } else {
    names.emplace_back(kEditTextTemplatesAction);
  }
  names.insert(names.end(), {kEditSymbolTableAction, kFloatingCompositionAction,
                             kUserGuideAction, kAboutAction});
  return names;
}

// What the menu does with a user file asked to be opened: a missing file is
// created first where that is allowed (the default folder), and otherwise not
// found, as it may not have been synced yet.
enum class UserFileOpening { Open, CreateThenOpen, NotFound };
inline UserFileOpening userFileOpening(bool exists, bool mayCreate) {
  if (exists) {
    return UserFileOpening::Open;
  }
  return mayCreate ? UserFileOpening::CreateThenOpen
                   : UserFileOpening::NotFound;
}

enum class SelectionKeys {
  Key_123456789,
  Key_asdfghjkl,
  Key_asdfzxcvb,
};
FCITX_CONFIG_ENUM_NAME_WITH_I18N(SelectionKeys, N_("123456789"),
                                 N_("asdfghjkl"), N_("asdfzxcvb"));

enum class CandidateLayoutHint { NotSet, Vertical, Horizontal };
FCITX_CONFIG_ENUM_NAME_WITH_I18N(CandidateLayoutHint, N_("Not Set"),
                                 N_("Vertical"), N_("Horizontal"));

enum class SelectPhrase { BeforeCursor, AfterCursor };
FCITX_CONFIG_ENUM_NAME_WITH_I18N(SelectPhrase, N_("before_cursor"),
                                 N_("after_cursor"));

enum class MovingCursorOption { Disabled, UseJK, UseHL };
FCITX_CONFIG_ENUM_NAME_WITH_I18N(MovingCursorOption,
                                 N_("Moving cursor is not allowed"),
                                 N_("J and K keys move cursor"),
                                 N_("H and L keys move cursor"));

enum class ShiftLetterKeys { DirectlyOutputUppercase, PutLowercaseToBuffer };
FCITX_CONFIG_ENUM_NAME_WITH_I18N(ShiftLetterKeys,
                                 N_("directly_output_uppercase"),
                                 N_("put_lowercase_to_buffer"));

constexpr char kDefaultOpenFileWith[] = "xdg-open";
constexpr char kDefaultAddPhraseHookPath[] =
    "/usr/share/fcitx5/data/mcbopomofo-add-phrase-hook.sh";

FCITX_CONFIG_ENUM_NAME_WITH_I18N(KeyHandlerCtrlEnter, N_("disabled"),
                                 N_("output_bpmf_reading"),
                                 N_("output_html_ruby_text"),
                                 N_("output_hanyu_pinyin"),
                                 N_("output_taiwan_braille_unicode"),
                                 N_("output_taiwan_braille_ascii"));

FCITX_CONFIGURATION(
    McBopomofoConfig,
    // Keyboard layout: standard, eten, etc.
    fcitx::OptionWithAnnotation<BopomofoKeyboardLayout,
                                BopomofoKeyboardLayoutI18NAnnotation>
        bopomofoKeyboardLayout{this, "BopomofoKeyboardLayout",
                               _("Bopomofo Keyboard Layout"),
                               BopomofoKeyboardLayout::Hsu};

    // Candidate layout.
    fcitx::OptionWithAnnotation<CandidateLayoutHint,
                                CandidateLayoutHintI18NAnnotation>
        candidateLayout{this, "CandidateLayout", _("Candidate List Layout"),
                        CandidateLayoutHint::Vertical};

    // Select selection keys.
    fcitx::OptionWithAnnotation<SelectionKeys, SelectionKeysI18NAnnotation>
        selectionKeys{this, "SelectionKeys", _("Selection Keys"),
                      SelectionKeys::Key_asdfghjkl};

    // Select count of selection keys.
    fcitx::Option<int, fcitx::IntConstrain> selectionKeysCount{
        this, "SelectionKeysCount", _("Selection Keys Count"), 9,
        fcitx::IntConstrain(4, 9)};

    // Space key chooses candidate.
    fcitx::Option<bool> chooseCandidateUsingSpace{
        this, "ChooseCandidateUsingSpace", _("Space key chooses candidate"),
        true};

    // Select the phrase as candidate before or after the cursor.
    fcitx::OptionWithAnnotation<SelectPhrase, SelectPhraseI18NAnnotation>
        selectPhrase{this, "SelectPhrase", _("Show Candidate Phrase"),
                     SelectPhrase::BeforeCursor};

    // Move the cursor at the end of the selected candidate phrase.
    fcitx::Option<bool> moveCursorAfterSelection{
        this, "MoveCursorAfterSelection", _("Move cursor after selection"),
        true};

    // This defines an option with annotation for MovingCursorOption
    // enumeration, which configures the cursor behavior when typing in
    // McBopomofo input method. The option allows users to customize how the
    // cursor moves after character selection.
    fcitx::OptionWithAnnotation<MovingCursorOption,
                                MovingCursorOptionI18NAnnotation>
        allowMovingCursorWhenChoosingCandidates{
            this, "AllowMovingCursorWhenChoosingCandidates",
            _("When Choosing Candidates"), MovingCursorOption::Disabled};

    // ESC key clears entire composing buffer.
    fcitx::Option<bool> escKeyClearsEntireComposingBuffer{
        this, "EscKeyClearsEntireComposingBuffer",
        _("ESC key clears entire composing buffer"), true};

    // ESC removes only the mistyped text before the cursor; pressing it twice
    // clears everything. When on, it replaces the option above.
    fcitx::Option<bool> escTrimsMistypedText{
        this, "EscTrimsMistypedText",
        _("ESC key removes only mistyped text at the end"), true};

    // Keep an uncomposable reading editable; a new Bopomofo component replaces
    // its existing tone marker so the user can continue the reading.
    fcitx::Option<bool> keepInvalidSyllableForFurtherInput{
        this, "KeepInvalidSyllableForFurtherInput",
        _("Keep invalid syllable for further input"), false};

    // Type English without Shift or Caps Lock: letters that cannot be
    // Bopomofo are put as English until a space, a symbol or Backspace.
    fcitx::Option<bool> smartChineseEnglishSwitchEnabled{
        this, "SmartChineseEnglishSwitchEnabled",
        _("Smart Chinese/English Switch: Type English without Shift or Caps "
          "Lock"),
        true};

    // Put a space between Chinese and English typed next to each other.
    fcitx::Option<bool> autoSpaceBetweenChineseAndEnglish{
        this, "AutoSpaceBetweenChineseAndEnglish",
        _("Add a space between Chinese and English"), true};

    // Punctuation right after English letters or digits is half width.
    fcitx::Option<bool> halfWidthPunctuationAfterEnglish{
        this, "HalfWidthPunctuationAfterEnglish",
        _("Half-width punctuation after English and digits"), true};

    // Fix the case of English words, like "iphone" to "iPhone", upon a space.
    fcitx::Option<bool> fixEnglishWordCase{
        this, "FixEnglishWordCase",
        _("Fix the case of words like iPhone upon a space"), true};

    // Remember the sentences committed often, suggest the rest of one after
    // its beginning is typed, and take it with Tab.
    fcitx::Option<bool> commonSentencesEnabled{
        this, "CommonSentencesEnabled",
        _("Remember sentences typed often, and complete them with Tab"), true};

    // Show a reminder ("ABC" or "中文") when Caps Lock switches the mode.
    fcitx::Option<bool> chineseEnglishSwitchReminderEnabled{
        this, "ChineseEnglishSwitchReminderEnabled",
        _("Show a reminder when switching between Chinese and English"), true};

    // Show the Chinese/English mode in the status area.
    fcitx::Option<bool> showChineseEnglishModeIcon{
        this, "ShowChineseEnglishModeIcon",
        _("Show the Chinese/English mode icon"), true};

    // Show the full/half width mode in the status area.
    fcitx::Option<bool> showFullHalfWidthModeIcon{
        this, "ShowFullHalfWidthModeIcon",
        _("Show the full/half width mode icon"), false};

    // Allow inputting Chinese when Caps Lock is on.
    fcitx::Option<bool> capsLockAllowChineseInput{
        this, "capsLockAllowChineseInput",
        _("Allow typing in Chinese while Caps Lock is on (like MS IME)"),
        false};

    // Shift + letter keys.
    fcitx::OptionWithAnnotation<ShiftLetterKeys, ShiftLetterKeysI18NAnnotation>
        shiftLetterKeys{this, "ShiftLetterKeys", _("Shift + Letter Keys"),
                        ShiftLetterKeys::DirectlyOutputUppercase};

    // Shift + Enter keys.
    fcitx::Option<bool> shiftEnterEnabled{
        this, "ShitEnterEnabled",
        _("Shift + Enter Key triggers associated phrases"), true};

    // Repeated punctuation to select next candidate.
    fcitx::Option<bool> repeatedPunctuationToSelectCandidateEnabled{
        this, "RepeatedPunctuationToSelectCandidateEnabled",
        _("Repeated punctuation to select next candidate"), true};

    // Ctrl + enter keys.
    fcitx::OptionWithAnnotation<KeyHandlerCtrlEnter,
                                KeyHandlerCtrlEnterI18NAnnotation>
        ctrlEnterKeys{this, "KeyHandlerCtrlEnter", _("Control + Enter Key"),
                      KeyHandlerCtrlEnter::OutputBpmfReadings};

    // Whether to show "Turn On/Off Bopomofo Font Annotation Support" in the
    // menu.
    fcitx::Option<bool> showBopomofoFontAnnotationSupportInMenu{
        this, "ShowBopomofoFontAnnotationSupportInMenu",
        _("Show the Bopomofo Font Annotation Support toggle in menu"), false};

    // The app to open custom phrase text files.
    fcitx::Option<std::string> openUserPhraseFilesWith{
        this, "OpenUserPhraseFilesWith", _("Open User Phrase Files With"),
        kDefaultOpenFileWith};

    // The path of the hook-script to add a phrase.
    fcitx::Option<std::string> addScriptHookPath{this, "AddScriptHookPath",
                                                 _("Add Phrase Hook Path"),
                                                 kDefaultAddPhraseHookPath};

    // If a script should be triggered when a new phrase is added.
    fcitx::Option<bool> addScriptHookEnabled{
        this, "AddScriptHookEnabled",
        _("Run the hook script after adding a phrase"), false};

    // The folder holding data.txt, exclude-phrases.txt and
    // phrases-replacement.txt. Empty means the default folder inside the
    // fcitx5 user directory. "~/" is expanded.
    fcitx::Option<std::string> userDataFolder{
        this, "UserDataFolder",
        _("User data folder (shared with other computers)"), ""};

    // The programs that show the text being composed in the input method
    // panel instead of inside the program, which only receives the committed
    // text. Names as the program reports them; also toggled from the menu.
    fcitx::Option<std::vector<std::string>> floatingCompositionPrograms{
        this, "FloatingCompositionPrograms",
        _("Programs using the floating composition window"), {}};

    // Identifies this device in the learned-choice folder. Generated on first
    // start; never edited by hand.
    fcitx::HiddenOption<std::string> deviceID{this, "DeviceID", "DeviceID", ""};

    // If half-width punctuation is enabled or not.
    fcitx::HiddenOption<bool> halfWidthPunctuationEnable{
        this, "HalfWidthPunctuationEnable", _("Enable Half Width Punctuation"),
        false};

    // If associated phrase is enabled or not.
    fcitx::HiddenOption<bool> associatedPhrasesEnabled{
        this, "AssociatedPhrasesEnabled", _("Enable Associated Phrases"),
        false};

    // Whether Bopomofo Font Annotation Support is enabled.
    fcitx::HiddenOption<bool> bopomofoFontAnnotationSupportEnabled{
        this, "BopomofoFontAnnotationSupportEnabled",
        _("Enable Bopomofo Font Annotation Support"), false};

    // Helps to open the user data directory.
    //
    // We have menu items in FCITX's input method to let the users to edit
    // the user phrases, however, the input menu is not visible on some
    // desktop environments, so we provide another button in the preference
    // dialog to open the user data directory.
    fcitx::ExternalOption userDataDir{
        this, "UserDataDir", _("User Data"),
        fcitx::stringutils::concat(
            "xdg-open \"",
            fcitx::stringutils::replaceAll(
                fcitx::stringutils::joinPath(
                    McBopomofo::fcitx5_compat::userDirectory(), "mcbopomofo"),
                "\"", "\"\"\""),
            "\"")};

    fcitx::ExternalOption userManual{
        this, "UserManual", _("User Manual"),
        fcitx::stringutils::concat("xdg-open \"", userGuideURL(kUserGuidePath),
                                   "\"")};

    fcitx::ExternalOption webTools{
        this, "WebTools", _("Web Tools"),
        fcitx::stringutils::concat(
            "xdg-open "
            "\"https://openvanilla.github.io/McBopomofoWeb/\"")};);

class McBopomofoEngine : public fcitx::InputMethodEngine {
 public:
  explicit McBopomofoEngine(fcitx::Instance* instance);
  ~McBopomofoEngine() override;
  fcitx::Instance* instance() { return instance_; }

  void activate(const fcitx::InputMethodEntry& entry,
                fcitx::InputContextEvent& event) override;
  void reset(const fcitx::InputMethodEntry& entry,
             fcitx::InputContextEvent& event) override;
  void deactivate(const fcitx::InputMethodEntry& entry,
                  fcitx::InputContextEvent& event) override;
  void keyEvent(const fcitx::InputMethodEntry& entry,
                fcitx::KeyEvent& keyEvent) override;

  const fcitx::Configuration* getConfig() const override;
  void setConfig(const fcitx::RawConfig& config) override;
  void reloadConfig() override;

 private:
  FCITX_ADDON_DEPENDENCY_LOADER(chttrans, instance_->addonManager());
  FCITX_ADDON_DEPENDENCY_LOADER(notifications, instance_->addonManager());
  fcitx::Instance* instance_;

  bool handleCandidateKeyEvent(
      fcitx::InputContext* context, fcitx::Key key, fcitx::Key origKey,
      fcitx::CommonCandidateList* candidateList,
      const McBopomofo::KeyHandler::StateCallback& stateCallback,
      const McBopomofo::KeyHandler::ErrorCallback& errorCallback);

  // The keys in the symbol table, which is a grid where a symbol is picked
  // with two keys: one for the row, and one for the symbol in the row.
  bool handleSymbolTableKeyEvent(
      fcitx::InputContext* context, fcitx::Key key, fcitx::Key origKey,
      const McBopomofo::KeyHandler::StateCallback& stateCallback,
      const McBopomofo::KeyHandler::ErrorCallback& errorCallback);
  // Shows the symbol table: the rows labeled with the keys, or the symbols
  // of the row picked.
  void showSymbolTable(fcitx::InputContext* context);
  // The selection key pressed with Shift, or -1, such as Shift+3 or Shift+D.
  int shiftedSelectionKeyIndex(const fcitx::Key& key,
                               const fcitx::Key& origKey) const;
  // The selection keys as characters, such as "123456789".
  std::string selectionKeyCharacters() const;

  // Leaves a menu opened from the candidate list: shows |choosing| again with
  // the candidate at |selectedIndex| highlighted.
  void returnToCandidateList(
      fcitx::InputContext* context,
      const McBopomofo::KeyHandler::StateCallback& stateCallback,
      const InputStates::ChoosingCandidate& choosing, size_t selectedIndex);

  // Handles state transitions.
  void enterNewState(fcitx::InputContext* context,
                     std::unique_ptr<InputState> newState);

  // Handles a single state or a StateSequence. If the incoming state is a
  // StateSequence, each state in the sequence is processed via enterNewState
  // in order.
  void handleStateOrSequence(fcitx::InputContext* context,
                             std::unique_ptr<InputState> newState);

  // Methods below enterNewState raw pointers as they don't affect ownership.
  void handleEmptyState(fcitx::InputContext* context, InputState* prev,
                        InputStates::Empty* current);
  void handleEmptyIgnoringPreviousState(
      fcitx::InputContext* context, InputState* prev,
      InputStates::EmptyIgnoringPrevious* current);
  void handleCommittingState(fcitx::InputContext* context, InputState* prev,
                             InputStates::Committing* current);
  void handleInputtingState(fcitx::InputContext* context, InputState* prev,
                            InputStates::Inputting* current);
  void handleCandidatesState(fcitx::InputContext* context, InputState* prev,
                             InputState* current);
  void handleMarkingState(fcitx::InputContext* context, InputState* prev,
                          InputStates::Marking* current);
  void handleStateWithCustomInput(fcitx::InputContext* context,
                                  std::string composingBuffer);

  // Helpers.

  // Updates the preedit with a not-empty state's composing buffer and
  // cursor index.
  void updatePreedit(fcitx::InputContext* context,
                     InputStates::NotEmpty* state);

  void showAndClearUserFileIssues();

  // Points the learned-choice store, the English words and the temporary
  // phrases at "<user data folder>/learning".
  void applyLearningFolder();
  // Gives the temporary phrases to the language model; none in Plain
  // Bopomofo.
  void applyTemporaryPhrases();
  // Reads every learning file that changed again now, for example after the
  // vocabulary manager wrote one.
  void recheckLearningFiles();
  // Points the text templates and the symbol table at the user data folder.
  void applyTextFiles();
  // Gives the symbol table to the language model, reading the file again if
  // it changed; none in Plain Bopomofo, which lists the punctuation instead.
  void applySymbolTable();
  // Writes the phrases to data.txt at once, reloads the user phrases and
  // tells the user. Returns the phrases learned.
  std::vector<SmartLearning::Phrase> learnUserPhrases(
      const std::vector<SmartLearning::Phrase>& phrases);
  // Resolves the configured user data folder and switches to it.
  void applyUserDataFolder();
  // Applies the options that take effect on the key handler right away, then
  // refreshes the mode icons.
  void applySmartOptions();
  // Brings the Chinese/English and full/half width actions up to date on
  // |context|, adding or removing them from the status area per the options.
  void refreshModeIcons(fcitx::InputContext* context);
  void updateChineseEnglishIcon(fcitx::InputContext* context);
  // Makes the standard/Hsu menu items' checked state follow the configured
  // layout.
  void refreshLayoutActions(fcitx::InputContext* context);
  // Whether to put the text being composed into the program of |context|,
  // rather than into the input method panel.
  bool useClientPreedit(fcitx::InputContext* context) const;
  // The floating composition menu item: shown only for a named program,
  // checked when that program uses it.
  void refreshFloatingCompositionAction(fcitx::InputContext* context);
  void toggleFloatingComposition(fcitx::InputContext* context);

  // Opens the text templates or the symbol table file at |path| with the
  // configured program, creating it first in the default folder (|create|
  // writes it); a missing file elsewhere is only told about, unless
  // |createInAnyFolder| (the symbol table, which starts as a copy of the
  // built-in one, as on the Mac) and the folder exists.
  void openUserFile(const std::filesystem::path& path,
                    const std::function<void()>& create,
                    bool createInAnyFolder = false);

  // Switches to |layout| from the menu: saves it, drops any composing text
  // without committing, and tells the user.
  void selectQuickLayout(fcitx::InputContext* context,
                         BopomofoKeyboardLayout layout, const char* title);
  void showUserDataFolderWarning();

  // (Re)starts the 2-second timer after which the learned choices are saved.
  // Choices made in quick succession share one save.
  void scheduleLearningSave();
  // Saves what has been learned and not saved yet. Returns false if anything
  // could not be saved.
  bool saveLearning();

  fcitx::CandidateLayoutHint getCandidateLayoutHint() const;

  std::shared_ptr<LanguageModelLoader> languageModelLoader_;
  std::vector<McBopomofoLM::UserFileIssue> userFileIssues_;
  std::shared_ptr<SmartLearning::UserOverrideStore> userOverrideStore_;
  std::shared_ptr<SmartLearning::EnglishWordStore> englishWordStore_;
  // The entries removed by the user, shared by the stores below.
  std::shared_ptr<SmartLearning::RemovedEntries> removedEntries_;
  std::shared_ptr<SmartLearning::TemporaryPhraseStore> temporaryPhraseStore_;
  std::shared_ptr<SmartLearning::CommonSentenceStore> commonSentenceStore_;
  std::shared_ptr<SmartLearning::TextTemplateStore> textTemplateStore_;
  SmartLearning::SymbolTableFile symbolTableFile_;
  std::vector<SmartLearning::SymbolCategory> symbolTable_;
  // The grid of the symbol table being shown.
  std::unique_ptr<SmartLearning::SymbolGrid> symbolGrid_;
  std::unique_ptr<fcitx::EventSourceTime> learningSaveTimer_;
  bool learningSaveRetried_ = false;
  // A choice has been learned since the learned choices were last saved.
  bool userOverrideSavePending_ = false;
  // The latest data-folder warning not shown yet, and the last one shown.
  std::string pendingDataFolderWarning_;
  std::string shownDataFolderWarning_;
  bool pendingDataFolderIsFallback_ = false;
  std::shared_ptr<KeyHandler> keyHandler_;
  std::unique_ptr<InputState> state_;
  McBopomofoConfig config_;
  fcitx::KeyList selectionKeys_;
  fcitx::KeyList numpadSelectionKeys_;

  std::unique_ptr<fcitx::SimpleAction> layoutStandardAction_;
  std::unique_ptr<fcitx::SimpleAction> layoutHsuAction_;
  std::unique_ptr<fcitx::SimpleAction> halfWidthPunctuationAction_;
  std::unique_ptr<fcitx::SimpleAction> chineseEnglishModeAction_;
  bool capsLockOn_ = false;
  // False after (de)activation until a key event tells the Caps Lock state.
  bool capsLockKnown_ = false;
  std::unique_ptr<fcitx::SimpleAction> associatedPhrasesAction_;
  std::unique_ptr<fcitx::SimpleAction> bopomofoFontAnnotationSupportAction_;
  std::unique_ptr<fcitx::SimpleAction> editUserPhrasesAction_;
  std::unique_ptr<fcitx::SimpleAction> excludedPhrasesAction_;
  // Only when the vocabulary manager is built.
  std::unique_ptr<fcitx::SimpleAction> manageVocabularyAction_;
  std::unique_ptr<fcitx::SimpleAction> floatingCompositionAction_;
  std::unique_ptr<fcitx::SimpleAction> editTextTemplatesAction_;
  std::unique_ptr<fcitx::SimpleAction> editSymbolTableAction_;
  std::unique_ptr<fcitx::SimpleAction> userGuideAction_;
  std::unique_ptr<fcitx::SimpleAction> aboutAction_;
};

class McBopomofoEngineFactory : public fcitx::AddonFactory {
  fcitx::AddonInstance* create(fcitx::AddonManager* manager) override {
    FCITX_UNUSED(manager);
    return new McBopomofoEngine(manager->instance());
  }
};

}  // namespace McBopomofo

#endif  // SRC_MCBOPOMOFO_H_
