#include "MixedInput.h"

#include <cctype>

namespace McBopomofo::SmartLearning {

namespace {

constexpr char kLiteralPrefix[] = "_literal_";
constexpr char kEnglishPrefix[] = "_english_";
constexpr char kNumpadPrefix[] = "_numpad_";

bool StartsWith(const std::string& s, const std::string& prefix) {
  return s.compare(0, prefix.size(), prefix) == 0;
}

bool IsDigitReading(const std::string& reading) {
  return IsHalfWidthAlphanumericReading(reading) &&
         !IsEnglishReading(reading);
}

}  // namespace

bool IsEnglishReading(const std::string& reading) {
  const std::string literal = kLiteralPrefix;
  if (StartsWith(reading, kEnglishPrefix)) {
    return true;
  }
  return reading.size() == literal.size() + 1 && StartsWith(reading, literal) &&
         std::isalpha(static_cast<unsigned char>(reading.back()));
}

bool IsHalfWidthAlphanumericReading(const std::string& reading) {
  if (IsEnglishReading(reading)) {
    return true;
  }
  const std::string literal = kLiteralPrefix;
  const std::string numpad = kNumpadPrefix;
  if ((reading.size() == literal.size() + 1 && StartsWith(reading, literal)) ||
      (reading.size() == numpad.size() + 1 && StartsWith(reading, numpad))) {
    return std::isdigit(static_cast<unsigned char>(reading.back()));
  }
  return false;
}

bool IsChineseReading(const std::string& reading) {
  const std::string literal = kLiteralPrefix;
  // A Chinese character put as is, such as from a common sentence.
  if (reading.size() == literal.size() + 3 && StartsWith(reading, literal)) {
    unsigned char lead = static_cast<unsigned char>(reading[literal.size()]);
    return lead >= 0xE3 && lead <= 0xE9;
  }
  return !reading.empty() && reading[0] != '_' && reading != " ";
}

bool IsLetterReading(const std::string& reading) {
  const std::string literal = kLiteralPrefix;
  return reading.size() == literal.size() + 1 && StartsWith(reading, literal) &&
         std::isalpha(static_cast<unsigned char>(reading.back()));
}

std::string TrailingLetters(const std::vector<std::string>& readings,
                            size_t cursor) {
  if (cursor > readings.size()) {
    cursor = readings.size();
  }
  size_t start = cursor;
  while (start > 0 && IsLetterReading(readings[start - 1])) {
    start--;
  }
  std::string run;
  for (size_t i = start; i < cursor; i++) {
    run += readings[i].back();
  }
  return run;
}

bool AutoSpaceNeeded(const std::vector<std::string>& readings, size_t cursor,
                     bool beforeEnglish) {
  if (cursor == 0 || cursor > readings.size()) {
    return false;
  }
  const std::string& previous = readings[cursor - 1];
  return beforeEnglish ? IsChineseReading(previous)
                       : IsEnglishReading(previous);
}

bool ShouldRemoveAutoSpaceAfterDeletingEnglish(
    const std::vector<std::string>& readings, size_t cursor) {
  if (cursor < 2 || cursor > readings.size() ||
      readings[cursor - 1] != kAutoSpaceReading ||
      !IsChineseReading(readings[cursor - 2])) {
    return false;
  }
  if (cursor < readings.size() && IsEnglishReading(readings[cursor])) {
    return false;
  }
  return true;
}

size_t PlanTrimBeforeCursor(
    const std::vector<std::string>& readings, size_t cursor,
    const std::function<bool(const std::string&)>& isWord) {
  if (cursor > readings.size()) {
    cursor = readings.size();
  }
  size_t deleted = 0;
  while (cursor > 0) {
    const std::string& last = readings[cursor - 1];
    if (IsChineseReading(last) ||
        (IsEnglishReading(last) && !IsLetterReading(last)) ||
        IsDigitReading(last)) {
      break;
    }
    if (!IsLetterReading(last)) {
      cursor--;
      deleted++;
      continue;
    }

    std::string run = TrailingLetters(readings, cursor);
    if (isWord(run)) {
      break;
    }
    // Keep a word at the beginning, like "hello" in "hellojkq". Two letters
    // are too often taken as a word, like "ls" in "lsqq".
    size_t keep = 0;
    for (size_t length = run.size() - 1; length >= 3; length--) {
      if (isWord(run.substr(0, length))) {
        keep = length;
        break;
      }
    }
    size_t remove = run.size() - keep;
    cursor -= remove;
    deleted += remove;
    if (keep) {
      break;
    }
  }
  return deleted;
}

std::string LanguageModeLabel(bool capsLockOn, bool smartEnglish) {
  return (capsLockOn || smartEnglish) ? "英" : "中";
}

std::optional<std::string> SwitchReminderText(bool previous, bool current) {
  if (previous == current) {
    return std::nullopt;
  }
  return current ? "ABC" : "中文";
}

std::string WidthModeLabel(bool halfWidth) { return halfWidth ? "半" : "全"; }

}  // namespace McBopomofo::SmartLearning
