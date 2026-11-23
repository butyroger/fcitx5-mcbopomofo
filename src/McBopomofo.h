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

#include <memory>
#include <string>
#include <type_traits>

#include "InputState.h"
#include "KeyHandler.h"
#include "LanguageModelLoader.h"
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
        "xdg-open "
        "\"https://github.com/openvanilla/fcitx5-mcbopomofo/wiki/使用手冊\""};

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

  // Points the learned-choice store and the English words at
  // "<user data folder>/learning".
  void applyLearningFolder();
  // Resolves the configured user data folder and switches to it.
  void applyUserDataFolder();
  // Applies the options that take effect on the key handler right away, then
  // refreshes the mode icons.
  void applySmartOptions();
  // Brings the Chinese/English and full/half width actions up to date on
  // |context|, adding or removing them from the status area per the options.
  void refreshModeIcons(fcitx::InputContext* context);
  void updateChineseEnglishIcon(fcitx::InputContext* context);
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

  std::unique_ptr<fcitx::SimpleAction> halfWidthPunctuationAction_;
  std::unique_ptr<fcitx::SimpleAction> chineseEnglishModeAction_;
  bool capsLockOn_ = false;
  std::unique_ptr<fcitx::SimpleAction> associatedPhrasesAction_;
  std::unique_ptr<fcitx::SimpleAction> bopomofoFontAnnotationSupportAction_;
  std::unique_ptr<fcitx::SimpleAction> editUserPhrasesAction_;
  std::unique_ptr<fcitx::SimpleAction> excludedPhrasesAction_;
};

class McBopomofoEngineFactory : public fcitx::AddonFactory {
  fcitx::AddonInstance* create(fcitx::AddonManager* manager) override {
    FCITX_UNUSED(manager);
    return new McBopomofoEngine(manager->instance());
  }
};

}  // namespace McBopomofo

#endif  // SRC_MCBOPOMOFO_H_
