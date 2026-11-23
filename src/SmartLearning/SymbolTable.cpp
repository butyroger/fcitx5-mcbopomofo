#include "SymbolTable.h"

#include <algorithm>
#include <utility>

namespace McBopomofo::SmartLearning {

namespace {

constexpr char kBuiltInSymbolTable[] =
    "標點\t　 ， 、 。 ． ‧ ； ： ？ ！ ︰ ⋯ ‥ … ｜ — ︴ ﹏ ＃ ＆ ＊ ※ § 〃 ＠ ～\n"
    "括號\t（ ） 「 」 『 』 《 》 〈 〉 【 】 〔 〕 ｛ ｝ ︵ ︶ ﹁ ﹂ ﹃ ﹄ ︽ ︾ ︿ ﹀ ︻ ︼ ︷ ︸ ︹ ︺\n"
    "標記\t○ ● ◎ ☆ ★ ◇ ◆ □ ■ △ ▲ ▽ ▼ ㊣ ♀ ♂ ✓ ✗ ⊙ ♠ ♥ ♦ ♣ ♪ ♫ ☎ ☞\n"
    "表情1\t😀 😃 😄 😁 😆 😅 🤣 😂 🙂 😉 😊 😇 🥰 😍 🤩 😘 😋 😛 😜 🤪\n"
    "表情2\t😝 🤗 🤭 🤫 🤔 😐 😑 😶 😏 😒 🙄 😬 😌 😔 😪 😴 😷 🤒 🤕 🥵\n"
    "表情3\t🥶 😵 🤯 🥳 😎 🤓 😕 😟 🙁 😮 😲 😳 🥺 😢 😭 😱 😤 😡\n"
    "手勢1\t👍 👎 👌 ✌️ 🤞 🤟 🤘 🤙 👈 👉 👆 👇 ☝️ ✋ 🤚 👋 👏 🙌 👐 🤲\n"
    "手勢2\t🙏 🤝 💪 ✍️ 💅 👀 🧠 👶 🙋\n"
    "愛心1\t❤️ 🧡 💛 💚 💙 💜 🖤 🤍 🤎 💔 ❣️ 💕 💞 💓 💗 💖 💘 💝 💋 🌹\n"
    "愛心2\t💐 🎁 🎉 🎊 🎈 🎂 ✨ ⭐ 🌟\n"
    "動植物1\t🐶 🐱 🐭 🐹 🐰 🦊 🐻 🐼 🐨 🐯 🦁 🐮 🐷 🐸 🐵 🐔 🐧 🐦 🐤 🦋\n"
    "動植物2\t🐝 🐞 🐢 🐟 🐳 🌸 🌷 🌻 🍀\n"
    "食物1\t🍎 🍊 🍋 🍌 🍉 🍇 🍓 🍒 🍑 🥭 🍍 🥝 🍅 🥑 🥦 🌽 🥕 🍞 🧀 🍳\n"
    "食物2\t🍔 🍟 🍕 🍜 🍣 🍱 🍰 🍩 ☕\n"
    "生活1\t🏠 🏢 🏥 🏪 🏫 🚗 🚕 🚌 🚲 🛵 🚄 ✈️ 🚢 📱 💻 ⌚ 📷 📞 ✉️ 📦\n"
    "生活2\t📝 📅 📌 📎 🔑 💡 💰 💳 🛒\n"
    "天氣1\t☀️ 🌤️ ⛅ ☁️ 🌧️ ⛈️ 🌩️ ❄️ ☃️ 🌈 🌙 ⭐ 🔥 💧 🌊 🌀 ⚡ 🌍 🌏 🎄\n"
    "天氣2\t🎃 🧧 🏮 🎆 🎇 🍾 🥂 🍻 🎵\n"
    "符號1\t✅ ❌ ⭕ ❗ ❓ ⚠️ 🚫 💯 🔴 🟠 🟡 🟢 🔵 🟣 ⚫ ⚪ 🆗 🆕 🆓 🔥\n"
    "符號2\t👉 ➡️ ⬅️ ⬆️ ⬇️ 🔝 🔜 ‼️ ⁉️\n"
    "箭頭\t→ ← ↑ ↓ ↗ ↖ ↘ ↙ ⇒ ⇔ ↔ ↕\n"
    "數學\t＋ － ± × ÷ ＝ ≠ ≒ ≡ ＜ ＞ ≦ ≧ ∞ √ ∴ ∵ ∠ ⊥ ∟ ⊿ ∩ ∪ ∫ ∼\n"
    "單位\t％ ‰ ° ℃ ℉ ＄ ￥ ￡ € ￠ ㎜ ㎝ ㎞ ㎡ ㎎ ㎏ ㏄ ㏕ ㏎ ㎖ ㎗ ㎘\n"
    "數字\t① ② ③ ④ ⑤ ⑥ ⑦ ⑧ ⑨ ⑩ ⑴ ⑵ ⑶ ⑷ ⑸ ⑹ ⑺ ⑻ ⑼ ⑽ Ⅰ Ⅱ Ⅲ Ⅳ Ⅴ Ⅵ Ⅶ Ⅷ Ⅸ Ⅹ 〡 〢 〣 〤 〥 〦 〧 〨 〩 十 卄 卅\n"
    "表格\t┌ ┬ ┐ ├ ┼ ┤ └ ┴ ┘ ─ │ ═ ║ ╔ ╦ ╗ ╠ ╬ ╣ ╚ ╩ ╝ ╞ ╪ ╡ ╭ ╮ ╰ ╯ ╱ ╲ ╳\n"
    "圖塊\t▁ ▂ ▃ ▄ ▅ ▆ ▇ █ ▏ ▎ ▍ ▌ ▋ ▊ ▉ ▔ ░ ▒ ▓\n"
    "希臘\tΑ Β Γ Δ Ε Ζ Η Θ Ι Κ Λ Μ Ν Ξ Ο Π Ρ Σ Τ Υ Φ Χ Ψ Ω α β γ δ ε ζ η θ ι κ λ μ ν ξ ο π ρ σ τ υ φ χ ψ ω\n"
    "注音\tㄅ ㄆ ㄇ ㄈ ㄉ ㄊ ㄋ ㄌ ㄍ ㄎ ㄏ ㄐ ㄑ ㄒ ㄓ ㄔ ㄕ ㄖ ㄗ ㄘ ㄙ ㄧ ㄨ ㄩ ㄚ ㄛ ㄜ ㄝ ㄞ ㄟ ㄠ ㄡ ㄢ ㄣ ㄤ ㄥ ㄦ ˙ ˊ ˇ ˋ\n";

constexpr std::string_view kByteOrderMark = "\xEF\xBB\xBF";

bool startsWith(std::string_view text, std::string_view prefix) {
  return text.substr(0, prefix.size()) == prefix;
}

// Removes the byte order marks, "*" and spaces at both ends of a title.
std::string_view trimTitle(std::string_view title) {
  bool trimmed = true;
  while (trimmed && !title.empty()) {
    trimmed = false;
    if (title.front() == '*' || title.front() == ' ') {
      title.remove_prefix(1);
      trimmed = true;
    } else if (startsWith(title, kByteOrderMark)) {
      title.remove_prefix(kByteOrderMark.size());
      trimmed = true;
    }
    if (title.empty()) {
      break;
    }
    if (title.back() == '*' || title.back() == ' ') {
      title.remove_suffix(1);
      trimmed = true;
    } else if (title.size() >= kByteOrderMark.size() &&
               title.substr(title.size() - kByteOrderMark.size()) ==
                   kByteOrderMark) {
      title.remove_suffix(kByteOrderMark.size());
      trimmed = true;
    }
  }
  return title;
}

// Removes the ASCII spaces, tabs and carriage returns at both ends.
std::string_view trimSymbol(std::string_view symbol) {
  auto isSpace = [](char c) { return c == ' ' || c == '\t' || c == '\r'; };
  while (!symbol.empty() && isSpace(symbol.front())) {
    symbol.remove_prefix(1);
  }
  while (!symbol.empty() && isSpace(symbol.back())) {
    symbol.remove_suffix(1);
  }
  return symbol;
}

}  // namespace

std::string_view BuiltInSymbolTable() { return kBuiltInSymbolTable; }

std::vector<SymbolCategory> ParseSymbolTable(std::string_view content) {
  std::vector<SymbolCategory> table;
  for (std::string_view line : splitLines(content)) {
    if (startsWith(line, "#") ||
        startsWith(line, std::string(kByteOrderMark) + "#")) {
      continue;
    }
    size_t tab = line.find('\t');
    if (tab == std::string_view::npos) {
      continue;
    }
    SymbolCategory category;
    category.title = std::string(trimTitle(line.substr(0, tab)));
    std::string_view rest = line.substr(tab + 1);
    while (true) {
      size_t space = rest.find(' ');
      std::string_view symbol = trimSymbol(rest.substr(0, space));
      if (!symbol.empty()) {
        category.symbols.emplace_back(symbol);
      }
      if (space == std::string_view::npos) {
        break;
      }
      rest.remove_prefix(space + 1);
    }
    if (!category.symbols.empty()) {
      table.push_back(std::move(category));
    }
  }
  return table;
}

std::vector<SymbolCategory> SymbolTableOfFile(
    const std::optional<std::string>& content) {
  if (!content || content->empty()) {
    return ParseSymbolTable(BuiltInSymbolTable());
  }
  return ParseSymbolTable(*content);
}

void SymbolTableFile::setPath(std::filesystem::path path) {
  if (path == path_) {
    return;
  }
  path_ = std::move(path);
  loaded_ = false;
}

bool SymbolTableFile::reloadIfChanged(std::vector<SymbolCategory>* table) {
  FileStamp stamp = stampOf(path_);
  if (loaded_ && stamp == stamp_) {
    return false;
  }
  loaded_ = true;
  stamp_ = stamp;
  std::optional<std::string> content;
  if (stamp.exists) {
    content = readWholeFile(path_);
  }
  *table = SymbolTableOfFile(content);
  return true;
}

std::string CandidateDisplayText(const std::string& value,
                                 const std::string& reading,
                                 size_t candidatesWithTheValue) {
  bool isSymbol = reading.rfind('_', 0) == 0;
  if (candidatesWithTheValue <= 1 || isSymbol) {
    return value;
  }
  std::string shownReading = reading;
  std::replace(shownReading.begin(), shownReading.end(), '-', ' ');
  return value + " (" + shownReading + ")";
}

SymbolGrid::SymbolGrid(const std::vector<SymbolSection>& sections,
                       size_t candidateCount, size_t columns, size_t keyCount)
    : candidateCount_(candidateCount), keyCount_(keyCount) {
  std::vector<SymbolSection> groups = sections;
  size_t total = 0;
  for (const auto& section : groups) {
    total += section.count;
  }
  if (total != candidateCount) {
    groups = {SymbolSection{"", candidateCount}};
  }
  columns = std::max<size_t>(columns, 1);
  size_t start = 0;
  for (const auto& group : groups) {
    for (size_t offset = 0; offset < group.count; offset += columns) {
      rows_.push_back(Row{offset == 0 ? group.title : std::string(),
                          start + offset,
                          std::min(columns, group.count - offset)});
    }
    start += group.count;
  }
}

size_t SymbolGrid::highlightedRow() const {
  for (size_t i = 0; i < rows_.size(); ++i) {
    if (highlighted_ >= rows_[i].start &&
        highlighted_ < rows_[i].start + rows_[i].count) {
      return i;
    }
  }
  return 0;
}

std::pair<size_t, size_t> SymbolGrid::labeledRowRange() const {
  if (rows_.empty()) {
    return {0, 0};
  }
  size_t first = highlightedRow();
  return {first, std::min(first + keyCount_, rows_.size())};
}

bool SymbolGrid::chooseRow(size_t keyIndex) {
  auto [first, last] = labeledRowRange();
  if (keyIndex >= last - first) {
    return false;
  }
  return chooseRowAt(first + keyIndex);
}

bool SymbolGrid::chooseRowAt(size_t row) {
  if (row >= rows_.size()) {
    return false;
  }
  highlighted_ = rows_[row].start;
  choosingRow_ = false;
  return true;
}

std::optional<size_t> SymbolGrid::candidateAt(size_t keyIndex) const {
  if (rows_.empty()) {
    return std::nullopt;
  }
  const Row& row = rows_[highlightedRow()];
  if (highlighted_ + keyIndex >= row.start + row.count) {
    return std::nullopt;
  }
  return highlighted_ + keyIndex;
}

void SymbolGrid::cancelChoosingCell() { choosingRow_ = true; }

bool SymbolGrid::setHighlighted(size_t index) {
  if (index >= candidateCount_) {
    return false;
  }
  highlighted_ = index;
  choosingRow_ = true;
  return true;
}

bool SymbolGrid::highlightNext() { return setHighlighted(highlighted_ + 1); }

bool SymbolGrid::highlightPrevious() {
  return highlighted_ > 0 && setHighlighted(highlighted_ - 1);
}

bool SymbolGrid::moveByRows(int rows) {
  if (rows_.empty()) {
    return false;
  }
  size_t current = highlightedRow();
  auto target = static_cast<long long>(current) + rows;
  if (target < 0 || target >= static_cast<long long>(rows_.size())) {
    return false;
  }
  size_t column = highlighted_ - rows_[current].start;
  const Row& row = rows_[static_cast<size_t>(target)];
  return setHighlighted(row.start + std::min(column, row.count - 1));
}

bool SymbolGrid::nextPage(size_t rowsPerPage) {
  if (rows_.empty()) {
    return false;
  }
  size_t rowsLeft = rows_.size() - 1 - highlightedRow();
  return moveByRows(static_cast<int>(std::min(std::max<size_t>(rowsPerPage, 1),
                                              rowsLeft))) ||
         setHighlighted(candidateCount_ - 1);
}

bool SymbolGrid::previousPage(size_t rowsPerPage) {
  size_t rowsBefore = highlightedRow();
  return moveByRows(-static_cast<int>(
      std::min(std::max<size_t>(rowsPerPage, 1), rowsBefore)));
}

bool SymbolGrid::highlightFirst() {
  return highlighted_ != 0 && setHighlighted(0);
}

bool SymbolGrid::highlightLast() {
  return candidateCount_ > 0 && highlighted_ != candidateCount_ - 1 &&
         setHighlighted(candidateCount_ - 1);
}

}  // namespace McBopomofo::SmartLearning
