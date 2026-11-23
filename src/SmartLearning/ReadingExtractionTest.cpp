#include "ReadingExtraction.h"

#include <gtest/gtest.h>

#include <string>

namespace McBopomofo::SmartLearning {

namespace {

class ReadingExtractionTest : public ::testing::Test {
 protected:
  static McBopomofoLM& lm() {
    static McBopomofoLM* instance = [] {
      auto* p = new McBopomofoLM();
      p->loadLanguageModel(MCBOPOMOFO_TEST_FULL_DATA_PATH);
      return p;
    }();
    return *instance;
  }
};

}  // namespace

TEST_F(ReadingExtractionTest, DataIsLoaded) {
  ASSERT_TRUE(lm().isDataModelLoaded());
}

TEST_F(ReadingExtractionTest, SingleCharacter) {
  EXPECT_EQ(ExtractReading(lm(), "你"), "ㄋㄧˇ");
}

TEST_F(ReadingExtractionTest, LongestPieceIsUsed) {
  // 你好 is one entry, whose reading is not made of the two characters'.
  EXPECT_EQ(ExtractReading(lm(), "你好"), "ㄋㄧˇ-ㄏㄠˇ");
}

TEST_F(ReadingExtractionTest, PiecesAreJoinedWithDash) {
  EXPECT_EQ(ExtractReading(lm(), "你好好"), "ㄋㄧˇ-ㄏㄠˇ-ㄏㄠˇ");
}

TEST_F(ReadingExtractionTest, HighestScoringReadingOfAPolyphone) {
  // 行 has ㄒㄧㄥˊ (-2.96), ㄏㄤˊ (-3.66), ㄒㄧㄥˋ and ㄏㄤˋ.
  EXPECT_EQ(ExtractReading(lm(), "行"), "ㄒㄧㄥˊ");
}

TEST_F(ReadingExtractionTest, UnknownCharacterGivesQuestionMark) {
  EXPECT_EQ(ExtractReading(lm(), "你𠀀好"), "ㄋㄧˇ-？-ㄏㄠˇ");
  EXPECT_EQ(ExtractReading(lm(), "𠀀你"), "？-ㄋㄧˇ");
}

TEST_F(ReadingExtractionTest, NothingFoundIsNullopt) {
  EXPECT_EQ(ExtractReading(lm(), "𠀀"), std::nullopt);
  EXPECT_EQ(ExtractReading(lm(), "𠀀𠀀"), std::nullopt);
  EXPECT_EQ(ExtractReading(lm(), ""), std::nullopt);
}

}  // namespace McBopomofo::SmartLearning
