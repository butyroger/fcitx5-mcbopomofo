#include "FileVocabulary.h"

#include <algorithm>

#include "AtomicFile.h"
#include "McBopomofoLM.h"
#include "ReadingExtraction.h"
#include "UserPhraseFile.h"
#include "VocabularyFile.h"

namespace McBopomofo::VocabularyManager {

namespace fs = std::filesystem;

namespace {

// The lines of |content| split at "\n", without the blank ones and those
// starting with "#".
std::vector<std::string_view> listedLines(std::string_view content) {
  std::vector<std::string_view> lines;
  size_t start = 0;
  while (start <= content.size()) {
    size_t end = content.find('\n', start);
    if (end == std::string_view::npos) {
      end = content.size();
    }
    std::string_view line = content.substr(start, end - start);
    start = end + 1;
    if (trimSpaces(line).empty() || line.front() == '#') {
      continue;
    }
    lines.push_back(line);
  }
  return lines;
}

// The length of the UTF-8 sequence starting at |text[i]|, and its code point.
size_t decode(std::string_view text, size_t i, char32_t* codePoint) {
  auto byte = static_cast<unsigned char>(text[i]);
  size_t length = byte < 0x80 ? 1 : byte < 0xE0 ? 2 : byte < 0xF0 ? 3 : 4;
  if (i + length > text.size()) {
    length = 1;
  }
  char32_t value = length == 1   ? byte
                   : length == 2 ? byte & 0x1F
                   : length == 3 ? byte & 0x0F
                                 : byte & 0x07;
  for (size_t k = 1; k < length; ++k) {
    value = (value << 6) | (static_cast<unsigned char>(text[i + k]) & 0x3F);
  }
  *codePoint = value;
  return length;
}

// A tab or a Unicode space separator.
bool isSpace(char32_t c) {
  return c == U'\t' || c == U' ' || c == 0x00A0 || c == 0x1680 ||
         (c >= 0x2000 && c <= 0x200A) || c == 0x202F || c == 0x205F ||
         c == 0x3000;
}

bool fileExists(const fs::path& file) {
  std::error_code ec;
  return fs::exists(file, ec);
}

}  // namespace

fs::path vocabularyFile(const fs::path& folder, FileKind kind) {
  switch (kind) {
    case FileKind::kPhrases:
      return folder / "data.txt";
    case FileKind::kExcluded:
      return folder / "exclude-phrases.txt";
    case FileKind::kTemplates:
      return folder / "text-templates.txt";
  }
  return {};
}

std::vector<VocabularyRow> parseRows(FileKind kind, std::string_view content) {
  std::vector<VocabularyRow> rows;
  auto lines = listedLines(content);
  for (size_t index = 0; index < lines.size(); ++index) {
    std::string_view line = lines[index];
    std::vector<std::string> columns;
    if (kind == FileKind::kTemplates) {
      size_t tab = line.find('\t');
      if (tab == std::string_view::npos) {
        continue;
      }
      columns = {std::string(line.substr(0, tab)),
                 std::string(line.substr(tab + 1))};
    } else {
      // Split once at a space; empty pieces before it are skipped.
      size_t begin = line.find_first_not_of(' ');
      size_t space = line.find(' ', begin);
      if (space == std::string_view::npos || space + 1 >= line.size()) {
        continue;
      }
      columns = {std::string(line.substr(begin, space - begin)),
                 std::string(line.substr(space + 1))};
    }
    rows.push_back(VocabularyRow{
        std::to_string(index) + "\t" + std::string(line), std::move(columns),
        std::string(line)});
  }
  return rows;
}

std::vector<VocabularyRow> loadRows(FileKind kind, const fs::path& file) {
  auto content = SmartLearning::readWholeFile(file);
  if (!content) {
    return {};
  }
  return parseRows(kind, *content);
}

std::string trimSpaces(std::string_view text) {
  size_t begin = 0;
  while (begin < text.size()) {
    char32_t c;
    size_t length = decode(text, begin, &c);
    if (!isSpace(c)) {
      break;
    }
    begin += length;
  }
  size_t end = begin;
  size_t i = begin;
  while (i < text.size()) {
    char32_t c;
    size_t length = decode(text, i, &c);
    i += length;
    if (!isSpace(c)) {
      end = i;
    }
  }
  return std::string(text.substr(begin, end - begin));
}

CommitResult commitEntry(FileKind kind, const fs::path& file,
                         std::string_view field1, std::string_view field2,
                         const std::optional<std::string>& editingKey) {
  std::string value1 = trimSpaces(field1);
  std::string value2 = trimSpaces(field2);
  CommitResult result{CommitStatus::kNothingTyped, value1};
  if (value1.empty()) {
    return result;
  }
  std::string line;
  if (kind == FileKind::kTemplates) {
    auto breaksLine = [](const std::string& text) {
      return text.find_first_of("\t\r\n") != std::string::npos;
    };
    if (value2.empty() || breaksLine(value1) || breaksLine(value2)) {
      result.status = CommitStatus::kBadTemplate;
      return result;
    }
    line = value1 + "\t" + value2;
  } else {
    // "？" marks a character whose reading was not found. Each column must
    // stay one column of the line.
    if (value2.find("？") != std::string::npos ||
        !SmartLearning::UserPhraseFile::fitsInColumn(value1) ||
        !SmartLearning::UserPhraseFile::fitsInColumn(value2)) {
      result.status = CommitStatus::kBadReading;
      return result;
    }
    line = value1 + " " + value2;
    if (!editingKey) {
      auto rows = loadRows(kind, file);
      if (std::any_of(rows.begin(), rows.end(),
                      [&](const VocabularyRow& row) { return row.key == line; })) {
        result.status = CommitStatus::kAlreadyListed;
        return result;
      }
    }
  }
  if (!fileExists(file)) {
    result.status = CommitStatus::kFileMissing;
    return result;
  }
  if (!SmartLearning::VocabularyFile::replaceOrAppendLine(file, line,
                                                          editingKey)) {
    result.status = fileExists(file) ? CommitStatus::kWriteFailed
                                     : CommitStatus::kFileMissing;
    return result;
  }
  result.status = editingKey ? CommitStatus::kSaved : CommitStatus::kAdded;
  return result;
}

DeleteStatus deleteEntries(const fs::path& file,
                           const std::vector<std::string>& keys) {
  if (keys.empty()) {
    return DeleteStatus::kNothingSelected;
  }
  if (!fileExists(file)) {
    return DeleteStatus::kFileMissing;
  }
  if (!SmartLearning::VocabularyFile::removeLines(file, keys)) {
    return fileExists(file) ? DeleteStatus::kWriteFailed
                            : DeleteStatus::kFileMissing;
  }
  return DeleteStatus::kDeleted;
}

ReadingFiller::ReadingFiller(fs::path dataFile)
    : dataFile_(std::move(dataFile)) {}

ReadingFiller::~ReadingFiller() = default;

std::string ReadingFiller::readingFor(std::string_view phrase) {
  std::string trimmed = trimSpaces(phrase);
  if (trimmed.empty()) {
    return {};
  }
  if (!loaded_) {
    loaded_ = true;
    if (!dataFile_.empty()) {
      auto lm = std::make_unique<McBopomofoLM>();
      lm->loadLanguageModel(dataFile_.c_str());
      if (lm->isDataModelLoaded()) {
        lm_ = std::move(lm);
      }
    }
  }
  if (!lm_) {
    return {};
  }
  return SmartLearning::ExtractReading(*lm_, trimmed).value_or("");
}

}  // namespace McBopomofo::VocabularyManager
