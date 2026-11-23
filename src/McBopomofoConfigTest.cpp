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

#include <gtest/gtest.h>

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
}

}  // namespace McBopomofo
