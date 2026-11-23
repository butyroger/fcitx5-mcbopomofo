#ifndef SRC_SMARTLEARNING_SYMBOLTABLE_H_
#define SRC_SMARTLEARNING_SYMBOLTABLE_H_

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "AtomicFile.h"
#include "McBopomofoLM.h"

namespace McBopomofo::SmartLearning {

using SymbolCategory = McBopomofo::McBopomofoLM::SymbolCategory;

// The symbol table used when the user data folder has none: each line is a
// category title, a tab, and the symbols separated by spaces.
std::string_view BuiltInSymbolTable();

// Each line of symbol-table.txt is a category title, a tab, and the symbols
// separated by ASCII spaces (a full-width space is a symbol). Lines starting
// with "#" (after an optional byte order mark) and lines without a tab are
// skipped. A trailing "*" after the title, used to mark a category by the
// symbol tables of other programs, is not a part of it. A category without
// symbols is skipped.
std::vector<SymbolCategory> ParseSymbolTable(std::string_view content);

// The table of the file content, or the built-in one if the file is missing
// or empty.
std::vector<SymbolCategory> SymbolTableOfFile(
    const std::optional<std::string>& content);

// symbol-table.txt, read again once it changed.
class SymbolTableFile {
 public:
  void setPath(std::filesystem::path path);

  // Reads the file again if it changed (or appeared, or went away) since the
  // last call, or if the path changed, and sets |*table|. Returns false,
  // leaving |*table| as it is, when nothing changed.
  bool reloadIfChanged(std::vector<SymbolCategory>* table);

 private:
  std::filesystem::path path_;
  bool loaded_ = false;
  FileStamp stamp_;
};

// The text of a candidate in a candidate list. A value that more than one
// candidate has is followed by its reading, so that they can be told apart,
// but not for a symbol (a reading starting with "_"): a symbol may be in
// more than one category of the symbol table.
std::string CandidateDisplayText(const std::string& value,
                                 const std::string& reading,
                                 size_t candidatesWithTheValue);

// The index of the typed character among the selection keys, ignoring case,
// so that Shift or Caps Lock with a letter key still picks, or nullopt.
std::optional<size_t> SelectionKeyIndexIgnoringCase(std::string_view keys,
                                                    char typed);

struct SymbolSection {
  std::string title;
  size_t count = 0;
};

// The symbol table shown as a grid, where a symbol is picked with two keys:
// the first picks a row, and the second a symbol in the row.
//
// Each section starts a new row, and a section longer than the number of
// columns takes more rows; only the first row of a section has its title.
// The keys label the rows from the highlighted one down, and then the
// symbols of the row picked, from its first one.
class SymbolGrid {
 public:
  struct Row {
    std::string title;  // Empty but for the first row of a section.
    size_t start = 0;   // The index of the first candidate of the row.
    size_t count = 0;
  };

  // |sections| give the titles and the counts of the candidates in order. If
  // their counts do not add up to |candidateCount|, all the candidates are
  // one section without a title. |keyCount| is the number of the keys that
  // pick a row or a symbol.
  SymbolGrid(const std::vector<SymbolSection>& sections, size_t candidateCount,
             size_t columns, size_t keyCount);

  [[nodiscard]] const std::vector<Row>& rows() const { return rows_; }
  [[nodiscard]] size_t candidateCount() const { return candidateCount_; }
  [[nodiscard]] size_t highlighted() const { return highlighted_; }
  [[nodiscard]] size_t highlightedRow() const;
  // Whether the next key picks a row (rather than a symbol in the row).
  [[nodiscard]] bool isChoosingRow() const { return choosingRow_; }
  // The rows labeled with the keys, [first, second): from the highlighted
  // row down, as many as there are keys.
  [[nodiscard]] std::pair<size_t, size_t> labeledRowRange() const;

  // Picks the row of the key: the highlight moves to its first symbol, and
  // the next key picks a symbol in it. False if no row has the key.
  bool chooseRow(size_t keyIndex);
  // Picks the row, such as one clicked.
  bool chooseRowAt(size_t row);
  // The candidate of the key in the row picked, or nullopt if the row is
  // shorter.
  [[nodiscard]] std::optional<size_t> candidateAt(size_t keyIndex) const;
  // Goes back to picking a row.
  void cancelChoosingCell();

  // Moving the highlight goes back to picking a row. Each returns false,
  // leaving everything as it is, when the highlight cannot move.
  bool setHighlighted(size_t index);
  bool highlightNext();
  bool highlightPrevious();
  // To the same column of another row, or the last symbol of a shorter row.
  bool moveByRows(int rows);
  // By |rowsPerPage| rows down, or to the last symbol.
  bool nextPage(size_t rowsPerPage);
  bool previousPage(size_t rowsPerPage);
  bool highlightFirst();
  bool highlightLast();

 private:
  std::vector<Row> rows_;
  size_t candidateCount_ = 0;
  size_t keyCount_ = 0;
  size_t highlighted_ = 0;
  bool choosingRow_ = true;
};

}  // namespace McBopomofo::SmartLearning

#endif  // SRC_SMARTLEARNING_SYMBOLTABLE_H_
