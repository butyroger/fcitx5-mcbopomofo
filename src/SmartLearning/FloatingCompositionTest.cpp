#include "FloatingComposition.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace McBopomofo::SmartLearning {

TEST(FloatingCompositionTest, ProgramInTheListDoesNotUseClientPreedit) {
  std::vector<std::string> programs{"xed", "texteditor"};
  EXPECT_TRUE(UsesFloatingComposition("xed", programs));
  EXPECT_FALSE(UseClientPreedit(true, "xed", programs));
  EXPECT_FALSE(UseClientPreedit(true, "texteditor", programs));
}

TEST(FloatingCompositionTest, OtherProgramsKeepTheClientPreedit) {
  std::vector<std::string> programs{"xed"};
  EXPECT_FALSE(UsesFloatingComposition("gedit", programs));
  EXPECT_TRUE(UseClientPreedit(true, "gedit", programs));
  // The names must match exactly.
  EXPECT_TRUE(UseClientPreedit(true, "Xed", programs));
  EXPECT_TRUE(UseClientPreedit(true, "xed2", programs));
  // Nothing is chosen.
  EXPECT_TRUE(UseClientPreedit(true, "xed", {}));
}

TEST(FloatingCompositionTest, EmptyProgramNeverApplies) {
  std::vector<std::string> programs{"", "xed"};
  EXPECT_FALSE(UsesFloatingComposition("", programs));
  EXPECT_TRUE(UseClientPreedit(true, "", programs));
  EXPECT_FALSE(UseClientPreedit(false, "", programs));
}

TEST(FloatingCompositionTest, WithoutClientPreeditNothingChanges) {
  std::vector<std::string> programs{"xed"};
  EXPECT_FALSE(UseClientPreedit(false, "xed", programs));
  EXPECT_FALSE(UseClientPreedit(false, "gedit", programs));
}

TEST(FloatingCompositionTest, ToggleAddsAtTheEnd) {
  auto result = ToggleFloatingComposition({"xed", "kate"}, "gedit");
  EXPECT_TRUE(result.enabled);
  EXPECT_EQ(result.programs,
            (std::vector<std::string>{"xed", "kate", "gedit"}));
}

TEST(FloatingCompositionTest, ToggleRemovesAndKeepsTheOrder) {
  auto result = ToggleFloatingComposition({"xed", "gedit", "kate"}, "gedit");
  EXPECT_FALSE(result.enabled);
  EXPECT_EQ(result.programs, (std::vector<std::string>{"xed", "kate"}));
}

TEST(FloatingCompositionTest, ToggleRemovesEveryCopy) {
  // A hand-edited list may hold a name twice.
  auto result =
      ToggleFloatingComposition({"gedit", "xed", "gedit"}, "gedit");
  EXPECT_FALSE(result.enabled);
  EXPECT_EQ(result.programs, (std::vector<std::string>{"xed"}));
  EXPECT_FALSE(UsesFloatingComposition("gedit", result.programs));
}

TEST(FloatingCompositionTest, ToggleTwiceRestoresTheList) {
  std::vector<std::string> programs{"xed"};
  auto on = ToggleFloatingComposition(programs, "kate");
  auto off = ToggleFloatingComposition(on.programs, "kate");
  EXPECT_TRUE(on.enabled);
  EXPECT_FALSE(off.enabled);
  EXPECT_EQ(off.programs, programs);
}

TEST(FloatingCompositionTest, ToggleWithEmptyProgramChangesNothing) {
  auto result = ToggleFloatingComposition({"xed"}, "");
  EXPECT_FALSE(result.enabled);
  EXPECT_EQ(result.programs, (std::vector<std::string>{"xed"}));
}

}  // namespace McBopomofo::SmartLearning
