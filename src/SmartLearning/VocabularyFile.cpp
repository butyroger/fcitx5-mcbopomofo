#include "VocabularyFile.h"

#include <algorithm>
#include <string_view>

#include "AtomicFile.h"
#include "LearningFileUtils.h"

namespace McBopomofo::SmartLearning::VocabularyFile {

namespace {

std::string join(const std::vector<std::string>& lines) {
  std::string result;
  for (size_t i = 0; i < lines.size(); ++i) {
    if (i > 0) {
      result += '\n';
    }
    result += lines[i];
  }
  return result;
}

std::vector<std::string> linesOf(const std::string& content) {
  std::vector<std::string> lines;
  for (std::string_view line : split(content, '\n', true)) {
    lines.emplace_back(line);
  }
  // split() of "" gives no part, where the file with no content has one
  // empty line.
  if (lines.empty()) {
    lines.emplace_back();
  }
  return lines;
}

}  // namespace

bool replaceOrAppendLine(const std::filesystem::path& path,
                         const std::string& newLine,
                         const std::optional<std::string>& oldLine) {
  std::error_code ec;
  if (!std::filesystem::exists(path, ec)) {
    return false;
  }
  std::optional<std::string> content = readWholeFile(path);
  if (!content) {
    return false;
  }

  if (oldLine) {
    std::vector<std::string> lines = linesOf(*content);
    auto it = std::find(lines.begin(), lines.end(), *oldLine);
    if (it != lines.end()) {
      if (*it == newLine) {
        // Saved unchanged: nothing to write.
        return true;
      }
      *it = newLine;
      return writeFileAtomically(path, join(lines));
    }
  }

  std::string result = std::move(*content);
  if (!result.empty() && result.back() != '\n') {
    result += '\n';
  }
  result += newLine;
  result += '\n';
  return writeFileAtomically(path, result);
}

bool removeLines(const std::filesystem::path& path,
                 const std::vector<std::string>& lines) {
  std::error_code ec;
  if (!std::filesystem::exists(path, ec)) {
    return false;
  }
  std::optional<std::string> content = readWholeFile(path);
  if (!content) {
    return false;
  }

  std::vector<std::string> remaining = lines;
  std::vector<std::string> kept;
  for (std::string& line : linesOf(*content)) {
    auto it = std::find(remaining.begin(), remaining.end(), line);
    if (it != remaining.end()) {
      remaining.erase(it);
      continue;
    }
    kept.push_back(std::move(line));
  }
  if (remaining.size() == lines.size()) {
    // Nothing matched: the file, which may be shared and synced, is left
    // untouched.
    return true;
  }
  return writeFileAtomically(path, join(kept));
}

}  // namespace McBopomofo::SmartLearning::VocabularyFile
