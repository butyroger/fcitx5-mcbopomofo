#include "LearnPhrase.h"

#include <gtest/gtest.h>

#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "UserOverrideModel.h"
#include "gramambular2/reading_grid.h"

namespace McBopomofo::SmartLearning {

using Formosa::Gramambular2::LanguageModel;
using Formosa::Gramambular2::ReadingGrid;

namespace {

constexpr double kNow = 1700000000;
constexpr size_t kCapacity = 500;
constexpr double kHalfLife = 30.0 * 86400.0;

// Synthetic data: 許是 wins over 許 + 是; 機 + 由 wins over 機油; the four
// characters 天下太平 win over the phrase.
constexpr char kData[] = R"(
ㄒㄩˇ 許 -3.0
ㄕˋ 是 -2.0
ㄕˋ 氏 -4.0
ㄒㄩˇ-ㄕˋ 許是 -4.0
ㄐㄧ 機 -3.0
ㄐㄧ 積 -3.7
ㄧㄡˊ 由 -3.0
ㄧㄡˊ 油 -3.7
ㄐㄧ-ㄧㄡˊ 機油 -8.0
ㄊㄧㄢ 天 -2.0
ㄒㄧㄚˋ 下 -2.0
ㄊㄞˋ 太 -2.0
ㄆㄧㄥˊ 平 -2.0
ㄊㄧㄢ-ㄒㄧㄚˋ-ㄊㄞˋ-ㄆㄧㄥˊ 天下太平 -20.0
_literal_a a 0
)";

class SimpleLM : public LanguageModel {
 public:
  explicit SimpleLM(const char* input) {
    std::stringstream ss(input);
    std::string line;
    while (std::getline(ss, line)) {
      if (line.empty()) {
        continue;
      }
      std::stringstream ls(line);
      std::string reading;
      std::string value;
      double score = 0;
      ls >> reading >> value >> score;
      db_[reading].emplace_back(value, score);
    }
  }
  std::vector<Unigram> getUnigrams(const std::string& key) override {
    auto it = db_.find(key);
    return it == db_.end() ? std::vector<Unigram>() : it->second;
  }
  bool hasUnigrams(const std::string& key) override {
    return db_.count(key) > 0;
  }

 private:
  std::map<std::string, std::vector<Unigram>> db_;
};

class LearnPhraseTest : public ::testing::Test {
 protected:
  LearnPhraseTest()
      : grid_(std::make_shared<SimpleLM>(kData)), model_(kCapacity, kHalfLife) {}

  // Types the readings, picks |value| at |cursor| and records the pick as the
  // key handler does. Returns the node picked.
  ReadingGrid::NodePtr pick(const std::vector<std::string>& readings,
                            size_t cursor, const std::string& value) {
    grid_.clear();
    for (const auto& r : readings) {
      grid_.insertReading(r);
    }
    before_ = grid_.walk().copyWithFixedNodes();
    EXPECT_TRUE(grid_.overrideCandidate(cursor, value));
    after_ = grid_.walk();
    auto it = after_.findNodeAt(cursor);
    EXPECT_NE(it, after_.nodes.cend());
    model_.observe(before_, after_, cursor, now_);
    now_ += 1;
    return *it;
  }

  ReadingGrid grid_;
  UserOverrideModel model_;
  ReadingGrid::WalkResult before_;
  ReadingGrid::WalkResult after_;
  double now_ = kNow;
};

}  // namespace

TEST_F(LearnPhraseTest, SingleCharacterIsNotLearned) {
  auto node = pick({"ㄐㄧ"}, 0, "積");
  EXPECT_EQ(PlanLearnPickedPhrase(node, model_).kind, LearnAction::kNone);
}

TEST_F(LearnPhraseTest, TwoCharacterPhraseIsTemporaryThenLearned) {
  auto node = pick({"ㄐㄧ", "ㄧㄡˊ"}, 1, "機油");
  ASSERT_EQ(before_.valuesAsStrings(), (std::vector<std::string>{"機", "由"}));
  LearnAction first = PlanLearnPickedPhrase(node, model_);
  EXPECT_EQ(first.kind, LearnAction::kAddTemporary);
  EXPECT_EQ(first.value, "機油");
  EXPECT_EQ(first.reading, "ㄐㄧ-ㄧㄡˊ");

  node = pick({"ㄐㄧ", "ㄧㄡˊ"}, 1, "機油");
  LearnAction second = PlanLearnPickedPhrase(node, model_);
  EXPECT_EQ(second.kind, LearnAction::kLearnUserPhrase);
  EXPECT_EQ(second.value, "機油");
  EXPECT_EQ(second.reading, "ㄐㄧ-ㄧㄡˊ");
}

TEST_F(LearnPhraseTest, PicksOfTheValueInOtherContextsCount) {
  // Picked once after 許 and once on its own: the value counts twice.
  pick({"ㄒㄩˇ", "ㄐㄧ", "ㄧㄡˊ"}, 2, "機油");
  auto node = pick({"ㄐㄧ", "ㄧㄡˊ"}, 1, "機油");
  EXPECT_EQ(PlanLearnPickedPhrase(node, model_).kind,
            LearnAction::kLearnUserPhrase);
}

TEST_F(LearnPhraseTest, PickedPhraseNotLearnedFromItsOwnBrokenUpPlan) {
  // A phrase is not a single character, so it never breaks a phrase up.
  auto node = pick({"ㄐㄧ", "ㄧㄡˊ"}, 1, "機油");
  EXPECT_EQ(PlanLearnBrokenUpPhrase(node, before_, after_, 1, model_).kind,
            LearnAction::kNone);
}

TEST_F(LearnPhraseTest, FourCharacterPhraseIsOnlyLearnedOnCommit) {
  const std::vector<std::string> readings = {"ㄊㄧㄢ", "ㄒㄧㄚˋ", "ㄊㄞˋ",
                                             "ㄆㄧㄥˊ"};
  for (int i = 0; i < 3; i++) {
    auto node = pick(readings, 3, "天下太平");
    LearnAction action = PlanLearnPickedPhrase(node, model_);
    EXPECT_EQ(action.kind, LearnAction::kAddTemporary) << i;
    EXPECT_EQ(action.value, "天下太平");
    EXPECT_EQ(action.reading, "ㄊㄧㄢ-ㄒㄧㄚˋ-ㄊㄞˋ-ㄆㄧㄥˊ");
  }

  // Typed again in a later composition, where the temporary phrase wins.
  std::set<Phrase> temporary = {{"天下太平", "ㄊㄧㄢ-ㄒㄧㄚˋ-ㄊㄞˋ-ㄆㄧㄥˊ"}};
  auto isTemporary = [&](const std::string& v, const std::string& r) {
    return temporary.count({v, r}) > 0;
  };
  EXPECT_EQ(PhrasesToLearnOnCommit(after_, {}, isTemporary),
            (std::vector<Phrase>{{"天下太平", "ㄊㄧㄢ-ㄒㄧㄚˋ-ㄊㄞˋ-ㄆㄧㄥˊ"}}));
  // Not when it was picked in the same composition.
  EXPECT_TRUE(PhrasesToLearnOnCommit(after_, temporary, isTemporary).empty());
}

TEST_F(LearnPhraseTest, BrokenUpPhraseIsTemporaryThenLearned) {
  auto node = pick({"ㄒㄩˇ", "ㄕˋ"}, 1, "氏");
  ASSERT_EQ(before_.valuesAsStrings(), (std::vector<std::string>{"許是"}));
  ASSERT_EQ(after_.valuesAsStrings(), (std::vector<std::string>{"許", "氏"}));
  // A single character is not learned on its own.
  EXPECT_EQ(PlanLearnPickedPhrase(node, model_).kind, LearnAction::kNone);
  LearnAction first = PlanLearnBrokenUpPhrase(node, before_, after_, 1, model_);
  EXPECT_EQ(first.kind, LearnAction::kAddTemporary);
  EXPECT_EQ(first.value, "許氏");
  EXPECT_EQ(first.reading, "ㄒㄩˇ-ㄕˋ");

  node = pick({"ㄒㄩˇ", "ㄕˋ"}, 1, "氏");
  LearnAction second =
      PlanLearnBrokenUpPhrase(node, before_, after_, 1, model_);
  EXPECT_EQ(second.kind, LearnAction::kLearnUserPhrase);
  EXPECT_EQ(second.value, "許氏");
  EXPECT_EQ(second.reading, "ㄒㄩˇ-ㄕˋ");
}

TEST_F(LearnPhraseTest, BrokenUpPlanNeedsAPhraseBefore) {
  // 機 + 由 was no phrase: picking 積 breaks nothing up.
  auto node = pick({"ㄐㄧ", "ㄧㄡˊ"}, 0, "積");
  EXPECT_EQ(PlanLearnBrokenUpPhrase(node, before_, after_, 0, model_).kind,
            LearnAction::kNone);
}

TEST_F(LearnPhraseTest, BrokenUpPlanSkipsLiteralReadings) {
  // 許是 broken up by a letter put as is.
  grid_.clear();
  grid_.insertReading("ㄒㄩˇ");
  grid_.insertReading("ㄕˋ");
  before_ = grid_.walk().copyWithFixedNodes();
  ASSERT_EQ(before_.valuesAsStrings(), (std::vector<std::string>{"許是"}));
  // Replace the walk after with one where a literal takes the place of 是.
  ReadingGrid other(std::make_shared<SimpleLM>(kData));
  other.insertReading("ㄒㄩˇ");
  other.insertReading("_literal_a");
  after_ = other.walk();
  auto node = *after_.findNodeAt(1);
  EXPECT_EQ(PlanLearnBrokenUpPhrase(node, before_, after_, 1, model_).kind,
            LearnAction::kNone);
}

TEST_F(LearnPhraseTest, CommitLearnsEachTemporaryPhraseOnce) {
  grid_.clear();
  for (const char* r : {"ㄒㄩˇ", "ㄕˋ", "ㄐㄧ", "ㄒㄩˇ", "ㄕˋ"}) {
    grid_.insertReading(r);
  }
  auto walk = grid_.walk();
  ASSERT_EQ(walk.valuesAsStrings(),
            (std::vector<std::string>{"許是", "機", "許是"}));
  int asked = 0;
  auto isTemporary = [&](const std::string& v, const std::string& r) {
    ++asked;
    return v == "許是" && r == "ㄒㄩˇ-ㄕˋ";
  };
  EXPECT_EQ(PhrasesToLearnOnCommit(walk, {}, isTemporary),
            (std::vector<Phrase>{{"許是", "ㄒㄩˇ-ㄕˋ"}}));
  // Single characters are not even looked up.
  EXPECT_EQ(asked, 1);
}

TEST_F(LearnPhraseTest, CommitSkipsWhatIsNotTemporary) {
  grid_.clear();
  grid_.insertReading("ㄒㄩˇ");
  grid_.insertReading("ㄕˋ");
  auto walk = grid_.walk();
  EXPECT_TRUE(PhrasesToLearnOnCommit(
                  walk, {},
                  [](const std::string&, const std::string&) { return false; })
                  .empty());
}

}  // namespace McBopomofo::SmartLearning
