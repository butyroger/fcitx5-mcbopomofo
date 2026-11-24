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

#include <fcitx-config/rawconfig.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <vector>

#include "McBopomofo.h"

namespace McBopomofo {

TEST(McBopomofoConfigTest, DefaultsMatchTheMacSetup) {
  McBopomofoConfig c;
  EXPECT_EQ(c.bopomofoKeyboardLayout.value(), BopomofoKeyboardLayout::Hsu);
  EXPECT_EQ(c.selectionKeys.value(), SelectionKeys::Key_asdfghjkl);
  EXPECT_EQ(c.candidateLayout.value(), CandidateLayoutHint::Vertical);
  EXPECT_TRUE(c.chooseCandidateUsingSpace.value());
  EXPECT_TRUE(c.moveCursorAfterSelection.value());
  EXPECT_TRUE(c.escKeyClearsEntireComposingBuffer.value());
  EXPECT_FALSE(c.keepInvalidSyllableForFurtherInput.value());
  EXPECT_TRUE(c.shiftEnterEnabled.value());
  EXPECT_TRUE(c.repeatedPunctuationToSelectCandidateEnabled.value());
  EXPECT_EQ(c.ctrlEnterKeys.value(), KeyHandlerCtrlEnter::OutputBpmfReadings);
  EXPECT_EQ(c.selectPhrase.value(), SelectPhrase::BeforeCursor);
  EXPECT_EQ(c.allowMovingCursorWhenChoosingCandidates.value(),
            MovingCursorOption::Disabled);
  EXPECT_FALSE(c.capsLockAllowChineseInput.value());
  EXPECT_EQ(c.deviceID.value(), "");
  EXPECT_EQ(c.userDataFolder.value(), "");
  EXPECT_TRUE(c.smartChineseEnglishSwitchEnabled.value());
  EXPECT_TRUE(c.autoSpaceBetweenChineseAndEnglish.value());
  EXPECT_TRUE(c.fixEnglishWordCase.value());
  EXPECT_TRUE(c.halfWidthPunctuationAfterEnglish.value());
  EXPECT_TRUE(c.escTrimsMistypedText.value());
  EXPECT_TRUE(c.chineseEnglishSwitchReminderEnabled.value());
  EXPECT_TRUE(c.showChineseEnglishModeIcon.value());
  EXPECT_FALSE(c.showFullHalfWidthModeIcon.value());
  EXPECT_TRUE(c.commonSentencesEnabled.value());
  EXPECT_TRUE(c.floatingCompositionPrograms.value().empty());
}

TEST(McBopomofoConfigTest, LayoutForConfig) {
  using L = Formosa::Mandarin::BopomofoKeyboardLayout;
  EXPECT_EQ(layoutForConfig(BopomofoKeyboardLayout::Standard),
            L::StandardLayout());
  EXPECT_EQ(layoutForConfig(BopomofoKeyboardLayout::Eten), L::ETenLayout());
  EXPECT_EQ(layoutForConfig(BopomofoKeyboardLayout::Hsu), L::HsuLayout());
  EXPECT_EQ(layoutForConfig(BopomofoKeyboardLayout::Et26), L::ETen26Layout());
  EXPECT_EQ(layoutForConfig(BopomofoKeyboardLayout::HanyuPinyin),
            L::HanyuPinyinLayout());
  EXPECT_EQ(layoutForConfig(BopomofoKeyboardLayout::IBM), L::IBMLayout());
}

TEST(McBopomofoConfigTest, QuickLayoutCheckedFollowsTheConfig) {
  EXPECT_TRUE(isQuickLayoutChecked(BopomofoKeyboardLayout::Standard,
                                   BopomofoKeyboardLayout::Standard));
  EXPECT_FALSE(isQuickLayoutChecked(BopomofoKeyboardLayout::Standard,
                                    BopomofoKeyboardLayout::Hsu));
  EXPECT_TRUE(isQuickLayoutChecked(BopomofoKeyboardLayout::Hsu,
                                   BopomofoKeyboardLayout::Hsu));
  // Another layout checks neither quick item.
  EXPECT_FALSE(isQuickLayoutChecked(BopomofoKeyboardLayout::Eten,
                                    BopomofoKeyboardLayout::Standard));
  EXPECT_FALSE(isQuickLayoutChecked(BopomofoKeyboardLayout::Eten,
                                    BopomofoKeyboardLayout::Hsu));
}

TEST(McBopomofoConfigTest, VocabularyManagerCommand) {
  EXPECT_EQ(vocabularyManagerCommand("/usr/bin/mcbopomofo-vocabulary",
                                     "/home/u/data folder", "ABCD1234"),
            (std::vector<std::string>{"/usr/bin/mcbopomofo-vocabulary",
                                      "--data-folder", "/home/u/data folder",
                                      "--device-id", "ABCD1234"}));
  // What is not known is left for the manager to read from the config.
  EXPECT_EQ(vocabularyManagerCommand("/usr/bin/mcbopomofo-vocabulary", "", ""),
            (std::vector<std::string>{"/usr/bin/mcbopomofo-vocabulary"}));
}

TEST(McBopomofoConfigTest, VocabularyManagerMenuItemWhenBuilt) {
#ifdef MCBOPOMOFO_VOCABULARY_MANAGER_PATH
  EXPECT_TRUE(kHasVocabularyManager);
  std::string path = kVocabularyManagerPath;
  EXPECT_EQ(path.substr(path.rfind('/') + 1), "mcbopomofo-vocabulary");
#else
  EXPECT_FALSE(kHasVocabularyManager);
#endif
}

TEST(McBopomofoConfigTest, MenuItemsInMcBopomofo) {
  EXPECT_EQ(userFileAndHelpMenuActions(InputMode::McBopomofo, true),
            (std::vector<std::string>{
                "mcbopomofo-manage-vocabulary",
                "mcbopomofo-symbol-table-edit",
                "mcbopomofo-floating-composition", "mcbopomofo-user-guide",
                "mcbopomofo-about"}));
}

TEST(McBopomofoConfigTest, MenuItemsInPlainBopomofo) {
  // No user phrase files of Plain Bopomofo; the rest as in McBopomofo.
  EXPECT_EQ(userFileAndHelpMenuActions(InputMode::PlainBopomofo, true),
            (std::vector<std::string>{
                "mcbopomofo-manage-vocabulary",
                "mcbopomofo-symbol-table-edit",
                "mcbopomofo-floating-composition", "mcbopomofo-user-guide",
                "mcbopomofo-about"}));
}

TEST(McBopomofoConfigTest, MenuItemsWithoutTheVocabularyManager) {
  auto names = userFileAndHelpMenuActions(InputMode::McBopomofo, false);
  EXPECT_EQ(std::count(names.begin(), names.end(),
                       std::string("mcbopomofo-manage-vocabulary")),
            0);
  // Without the vocabulary manager, the user phrase, excluded phrase and text
  // templates files are opened from the menu instead.
  EXPECT_EQ(std::count(names.begin(), names.end(),
                       std::string("mcbopomofo-user-phrases-edit")),
            1);
  EXPECT_EQ(std::count(names.begin(), names.end(),
                       std::string("mcbopomofo-user-excluded-phrases-edit")),
            1);
  EXPECT_EQ(std::count(names.begin(), names.end(),
                       std::string("mcbopomofo-text-templates-edit")),
            1);
  EXPECT_EQ(names.size(), 7U);
}

TEST(McBopomofoConfigTest, UserGuideURL) {
  EXPECT_EQ(userGuideURL("/usr/share/fcitx5-mcbopomofo/user-guide.html"),
            "file:///usr/share/fcitx5-mcbopomofo/user-guide.html");
  EXPECT_EQ(
      userGuideURL("/usr/share/fcitx5-mcbopomofo/user-guide.html", "about"),
      "file:///usr/share/fcitx5-mcbopomofo/user-guide.html#about");
  std::string path = kUserGuidePath;
  EXPECT_EQ(path.substr(path.rfind('/') + 1), "user-guide.html");
}

TEST(McBopomofoConfigTest, UserManualOpensTheInstalledGuide) {
  McBopomofoConfig c;
  fcitx::RawConfig description;
  c.userManual.dumpDescription(description);
  const std::string* command = description.valueByPath("External");
  ASSERT_NE(command, nullptr);
  EXPECT_EQ(*command, "xdg-open \"" + userGuideURL(kUserGuidePath) + "\"");
}

TEST(McBopomofoConfigTest, UserFileOpening) {
  EXPECT_EQ(userFileOpening(true, true), UserFileOpening::Open);
  EXPECT_EQ(userFileOpening(true, false), UserFileOpening::Open);
  EXPECT_EQ(userFileOpening(false, true), UserFileOpening::CreateThenOpen);
  // A folder chosen by the user may still be syncing.
  EXPECT_EQ(userFileOpening(false, false), UserFileOpening::NotFound);
}

}  // namespace McBopomofo
