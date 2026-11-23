#include "SymbolTable.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "gtest/gtest.h"

namespace McBopomofo::SmartLearning {
namespace {

using Strings = std::vector<std::string>;

TEST(SymbolTableTest, ParsesCategories) {
  auto table = ParseSymbolTable(
      "# comment\tnot a category\n"
      "\xEF\xBB\xBF# comment after a byte order mark\tno\n"
      "Arrows\t\xE2\x86\x92 \xE2\x86\x90  \xE2\x86\x91\r\n"
      "no tab\n"
      "Empty\t   \n"
      "Spaces\t\xE3\x80\x80 x\n");
  ASSERT_EQ(table.size(), 2u);
  EXPECT_EQ(table[0].title, "Arrows");
  EXPECT_EQ(table[0].symbols, (Strings{"→", "←", "↑"}));
  EXPECT_EQ(table[1].title, "Spaces");
  // The full-width space is a symbol.
  EXPECT_EQ(table[1].symbols, (Strings{"\xE3\x80\x80", "x"}));
}

TEST(SymbolTableTest, TitleDropsMarkersAndByteOrderMark) {
  auto table = ParseSymbolTable(
      "\xEF\xBB\xBF" "Punctuation*\t, .\n"
      " * Math * \t+ -\n");
  ASSERT_EQ(table.size(), 2u);
  EXPECT_EQ(table[0].title, "Punctuation");
  EXPECT_EQ(table[1].title, "Math");
}

TEST(SymbolTableTest, BuiltInTable) {
  auto table = SymbolTableOfFile(std::nullopt);
  ASSERT_EQ(table.size(), 28u);
  EXPECT_EQ(table.front().title, "標點");
  EXPECT_EQ(table.front().symbols.front(), "\xE3\x80\x80");
  EXPECT_EQ(table.front().symbols[1], "，");
  EXPECT_EQ(table.back().title, "注音");
  EXPECT_EQ(table.back().symbols.back(), "ˋ");
  // An empty file gives the built-in table too.
  EXPECT_EQ(SymbolTableOfFile(std::string()).size(), 28u);
  // A file with only comments gives no table.
  EXPECT_TRUE(SymbolTableOfFile(std::string("# nothing\n")).empty());
}

TEST(SymbolTableTest, DuplicatedSymbolShowsNoReading) {
  EXPECT_EQ(CandidateDisplayText("⭐", "_symbols_", 2), "⭐");
  EXPECT_EQ(CandidateDisplayText("，", "_punctuation_list", 3), "，");
  EXPECT_EQ(CandidateDisplayText("中", "ㄓㄨㄥ", 1), "中");
  EXPECT_EQ(CandidateDisplayText("中", "ㄓㄨㄥ-ㄓㄨㄥ", 2), "中 (ㄓㄨㄥ ㄓㄨㄥ)");
}

TEST(SymbolTableTest, SelectionKeysIgnoreCase) {
  EXPECT_EQ(SelectionKeyIndexIgnoringCase("asdfghjkl", 'd'), 2u);
  EXPECT_EQ(SelectionKeyIndexIgnoringCase("asdfghjkl", 'D'), 2u);
  EXPECT_EQ(SelectionKeyIndexIgnoringCase("123456789", '3'), 2u);
  // Shift and a digit is another character.
  EXPECT_EQ(SelectionKeyIndexIgnoringCase("123456789", '#'), std::nullopt);
  EXPECT_EQ(SelectionKeyIndexIgnoringCase("asdfghjkl", 'q'), std::nullopt);
  EXPECT_EQ(SelectionKeyIndexIgnoringCase("asdfghjkl", 0), std::nullopt);
}

class SymbolTableFileTest : public ::testing::Test {
 protected:
  void SetUp() override {
    auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    folder_ = std::filesystem::temp_directory_path() /
              ("symbol-table-" + std::to_string(stamp));
    std::filesystem::create_directories(folder_);
    path_ = folder_ / "symbol-table.txt";
  }
  void TearDown() override {
    std::error_code ec;
    std::filesystem::remove_all(folder_, ec);
  }
  void write(const std::string& content) {
    std::ofstream out(path_, std::ios::binary | std::ios::trunc);
    out << content;
  }
  std::filesystem::path folder_;
  std::filesystem::path path_;
};

TEST_F(SymbolTableFileTest, ReadsWhenChanged) {
  SymbolTableFile file;
  file.setPath(path_);
  std::vector<SymbolCategory> table;
  ASSERT_TRUE(file.reloadIfChanged(&table));
  EXPECT_EQ(table.size(), 28u);
  EXPECT_FALSE(file.reloadIfChanged(&table));

  write("Mine\ta b\n");
  ASSERT_TRUE(file.reloadIfChanged(&table));
  ASSERT_EQ(table.size(), 1u);
  EXPECT_EQ(table[0].title, "Mine");
  EXPECT_FALSE(file.reloadIfChanged(&table));

  std::filesystem::remove(path_);
  ASSERT_TRUE(file.reloadIfChanged(&table));
  EXPECT_EQ(table.size(), 28u);
}

// Two sections: "A" with 5 symbols and "B" with 12, in a grid of 4 columns
// picked with 3 keys.
//   row 0: A  0 1 2 3
//   row 1:    4
//   row 2: B  5 6 7 8
//   row 3:    9 10 11 12
//   row 4:    13 14 15 16
SymbolGrid makeGrid() {
  return SymbolGrid({{"A", 5}, {"B", 12}}, 17, 4, 3);
}

TEST(SymbolGridTest, Rows) {
  SymbolGrid grid = makeGrid();
  const auto& rows = grid.rows();
  ASSERT_EQ(rows.size(), 5u);
  EXPECT_EQ(rows[0].title, "A");
  EXPECT_EQ(rows[0].start, 0u);
  EXPECT_EQ(rows[0].count, 4u);
  EXPECT_EQ(rows[1].title, "");
  EXPECT_EQ(rows[1].count, 1u);
  EXPECT_EQ(rows[2].title, "B");
  EXPECT_EQ(rows[2].start, 5u);
  EXPECT_EQ(rows[4].start, 13u);
  EXPECT_EQ(rows[4].count, 4u);
  EXPECT_TRUE(grid.isChoosingRow());
  EXPECT_EQ(grid.labeledRowRange(), (std::pair<size_t, size_t>{0, 3}));
}

TEST(SymbolGridTest, SectionsNotAddingUpAreOneSection) {
  SymbolGrid grid({{"A", 5}}, 6, 4, 3);
  ASSERT_EQ(grid.rows().size(), 2u);
  EXPECT_EQ(grid.rows()[0].title, "");
  EXPECT_EQ(grid.rows()[1].count, 2u);
}

TEST(SymbolGridTest, TwoKeysPickASymbol) {
  SymbolGrid grid = makeGrid();
  ASSERT_TRUE(grid.chooseRow(2));
  EXPECT_FALSE(grid.isChoosingRow());
  EXPECT_EQ(grid.highlighted(), 5u);
  EXPECT_EQ(grid.candidateAt(0), 5u);
  EXPECT_EQ(grid.candidateAt(3), 8u);
  // Past the end of the row.
  EXPECT_EQ(grid.candidateAt(4), std::nullopt);
}

TEST(SymbolGridTest, RowKeysStartFromTheHighlightedRow) {
  SymbolGrid grid = makeGrid();
  ASSERT_TRUE(grid.setHighlighted(10));
  EXPECT_EQ(grid.labeledRowRange(), (std::pair<size_t, size_t>{3, 5}));
  // Only two rows are left to be labeled.
  EXPECT_FALSE(grid.chooseRow(2));
  EXPECT_TRUE(grid.isChoosingRow());
  ASSERT_TRUE(grid.chooseRow(1));
  EXPECT_EQ(grid.highlighted(), 13u);
}

TEST(SymbolGridTest, BackspaceGoesBackToTheRows) {
  SymbolGrid grid = makeGrid();
  ASSERT_TRUE(grid.chooseRow(1));
  EXPECT_EQ(grid.candidateAt(1), std::nullopt);
  grid.cancelChoosingCell();
  EXPECT_TRUE(grid.isChoosingRow());
  EXPECT_EQ(grid.labeledRowRange(), (std::pair<size_t, size_t>{1, 4}));
}

TEST(SymbolGridTest, MovingGoesBackToTheRows) {
  SymbolGrid grid = makeGrid();
  ASSERT_TRUE(grid.chooseRow(0));
  ASSERT_TRUE(grid.highlightNext());
  EXPECT_TRUE(grid.isChoosingRow());
  EXPECT_EQ(grid.highlighted(), 1u);
  EXPECT_FALSE(grid.moveByRows(-1));
  // Down to a shorter row takes its last symbol.
  ASSERT_TRUE(grid.setHighlighted(3));
  ASSERT_TRUE(grid.moveByRows(1));
  EXPECT_EQ(grid.highlighted(), 4u);
  ASSERT_TRUE(grid.moveByRows(1));
  EXPECT_EQ(grid.highlighted(), 5u);
  ASSERT_TRUE(grid.setHighlighted(7));
  ASSERT_TRUE(grid.moveByRows(2));
  EXPECT_EQ(grid.highlighted(), 15u);
  EXPECT_FALSE(grid.moveByRows(1));
  ASSERT_TRUE(grid.highlightPrevious());
  EXPECT_EQ(grid.highlighted(), 14u);
  ASSERT_TRUE(grid.setHighlighted(0));
  EXPECT_FALSE(grid.highlightPrevious());
  EXPECT_FALSE(grid.highlightFirst());
  ASSERT_TRUE(grid.highlightLast());
  EXPECT_EQ(grid.highlighted(), 16u);
  EXPECT_FALSE(grid.highlightNext());
  EXPECT_FALSE(grid.setHighlighted(17));
}

TEST(SymbolGridTest, Pages) {
  SymbolGrid grid = makeGrid();
  ASSERT_TRUE(grid.setHighlighted(1));
  ASSERT_TRUE(grid.nextPage(3));
  EXPECT_EQ(grid.highlighted(), 10u);  // Row 3, the same column.
  ASSERT_TRUE(grid.nextPage(3));
  EXPECT_EQ(grid.highlighted(), 14u);  // Only one row left.
  ASSERT_TRUE(grid.previousPage(3));
  EXPECT_EQ(grid.highlighted(), 4u);  // Row 1, which has one symbol only.
  ASSERT_TRUE(grid.previousPage(3));
  EXPECT_EQ(grid.highlighted(), 0u);
}

TEST(SymbolGridTest, EmptyGrid) {
  SymbolGrid grid({}, 0, 4, 3);
  EXPECT_TRUE(grid.rows().empty());
  EXPECT_FALSE(grid.chooseRow(0));
  EXPECT_EQ(grid.candidateAt(0), std::nullopt);
  EXPECT_FALSE(grid.nextPage(3));
  EXPECT_FALSE(grid.moveByRows(1));
  EXPECT_FALSE(grid.highlightLast());
}

}  // namespace
}  // namespace McBopomofo::SmartLearning
