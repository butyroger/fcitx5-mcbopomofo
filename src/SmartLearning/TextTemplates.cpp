#include "TextTemplates.h"

#include <unicode/brkiter.h>
#include <unicode/locid.h>
#include <unicode/uchar.h>
#include <unicode/unistr.h>

#include <cstring>
#include <memory>
#include <optional>
#include <utility>

#include "QuickNumbers.h"

namespace McBopomofo::SmartLearning {

namespace {

constexpr size_t kMaxPreviewLength = 30;

bool isTrimmedSpace(UChar32 c) {
  return c == '\t' || u_charType(c) == U_SPACE_SEPARATOR;
}

// Removes the spaces (any space separator and tab) at both ends.
std::string trimSpaces(std::string_view text) {
  icu::UnicodeString s = icu::UnicodeString::fromUTF8(
      icu::StringPiece(text.data(), static_cast<int32_t>(text.size())));
  int32_t start = 0;
  int32_t end = s.length();
  while (start < end && isTrimmedSpace(s.char32At(start))) {
    start = s.moveIndex32(start, 1);
  }
  while (end > start) {
    int32_t previous = s.moveIndex32(end, -1);
    if (!isTrimmedSpace(s.char32At(previous))) {
      break;
    }
    end = previous;
  }
  std::string result;
  s.tempSubStringBetween(start, end).toUTF8String(result);
  return result;
}

std::string unescape(std::string_view text) {
  std::string result;
  bool escaping = false;
  for (char c : text) {
    if (escaping) {
      if (c == 'n') {
        result += '\n';
      } else if (c == 't') {
        result += '\t';
      } else {
        result += c;
      }
      escaping = false;
    } else if (c == '\\') {
      escaping = true;
    } else {
      result += c;
    }
  }
  if (escaping) {
    result += '\\';
  }
  return result;
}

std::string lowercased(const std::string& text) {
  icu::UnicodeString s = icu::UnicodeString::fromUTF8(text);
  s.toLower(icu::Locale::getRoot());
  std::string result;
  s.toUTF8String(result);
  return result;
}

std::string caseFolded(const std::string& text) {
  icu::UnicodeString s = icu::UnicodeString::fromUTF8(text);
  s.foldCase();
  std::string result;
  s.toUTF8String(result);
  return result;
}

bool startsWith(const std::string& text, const std::string& prefix) {
  return text.compare(0, prefix.size(), prefix) == 0;
}

bool contains(const std::string& text, const std::string& part) {
  return text.find(part) != std::string::npos;
}

// The text cut to |limit| characters (as the user sees them, so that an emoji
// made of several code points counts as one) with "…", or the text as is if
// it is not longer.
std::string cutToCharacters(const std::string& text, size_t limit) {
  icu::UnicodeString s = icu::UnicodeString::fromUTF8(text);
  UErrorCode status = U_ZERO_ERROR;
  std::unique_ptr<icu::BreakIterator> characters(
      icu::BreakIterator::createCharacterInstance(icu::Locale::getRoot(),
                                                  status));
  if (U_FAILURE(status) || characters == nullptr) {
    return text;
  }
  characters->setText(s);
  characters->first();
  size_t count = 0;
  int32_t cut = 0;
  for (int32_t next = characters->next(); next != icu::BreakIterator::DONE;
       next = characters->next()) {
    ++count;
    if (count == limit) {
      cut = next;
    } else if (count > limit) {
      std::string result;
      s.tempSubStringBetween(0, cut).toUTF8String(result);
      return result + "…";
    }
  }
  return text;
}

}  // namespace

std::vector<TextTemplateEntry> ParseTextTemplates(std::string_view content) {
  std::vector<TextTemplateEntry> entries;
  for (std::string_view line : splitLines(content)) {
    if (!line.empty() && line.front() == '#') {
      continue;
    }
    size_t tab = line.find('\t');
    if (tab == std::string_view::npos) {
      continue;
    }
    std::string code = trimSpaces(line.substr(0, tab));
    std::string text = unescape(line.substr(tab + 1));
    if (!code.empty() && !text.empty()) {
      entries.push_back({std::move(code), std::move(text)});
    }
  }
  return entries;
}

std::vector<TextTemplateEntry> MatchTextTemplates(
    const std::vector<TextTemplateEntry>& templates, std::string_view code) {
  if (code.empty()) {
    return templates;
  }
  std::string typed = lowercased(std::string(code));
  std::string foldedTyped = caseFolded(std::string(code));
  std::vector<TextTemplateEntry> exact;
  std::vector<TextTemplateEntry> prefix;
  std::vector<TextTemplateEntry> others;
  for (const auto& entry : templates) {
    std::string entryCode = lowercased(entry.code);
    if (entryCode == typed) {
      exact.push_back(entry);
    } else if (startsWith(entryCode, typed)) {
      prefix.push_back(entry);
    } else if (contains(entryCode, typed) ||
               contains(caseFolded(entry.text), foldedTyped)) {
      others.push_back(entry);
    }
  }
  exact.insert(exact.end(), prefix.begin(), prefix.end());
  exact.insert(exact.end(), others.begin(), others.end());
  return exact;
}

std::vector<TextTemplateEntry> TextTemplateCandidates(
    const std::vector<TextTemplateEntry>& templates, const std::string& code,
    std::time_t now) {
  if (IsNumberCode(code)) {
    std::vector<TextTemplateEntry> numbers;
    for (auto& text : QuickNumberCandidates(code, now)) {
      numbers.push_back({"", std::move(text)});
    }
    return numbers;
  }
  return MatchTextTemplates(templates, code);
}

std::string TextTemplateDisplayText(const TextTemplateEntry& entry) {
  if (entry.code.empty()) {
    return entry.text;
  }
  std::string preview;
  for (char c : entry.text) {
    if (c == '\n') {
      preview += "⏎";
    } else {
      preview += c;
    }
  }
  return entry.code + "  " + cutToCharacters(preview, kMaxPreviewLength);
}

bool IsNumberOperatorKey(const std::string& code, char key) {
  return key != 0 && std::strchr("*()+:", key) != nullptr &&
         IsNumberCode(code);
}

TextTemplateStore::TextTemplateStore(std::function<double()> now)
    : now_(std::move(now)) {}

void TextTemplateStore::setFile(std::filesystem::path path,
                                std::string sample) {
  path_ = std::move(path);
  sample_ = std::move(sample);
  checked_ = false;
  stamp_ = FileStamp();
  entries_.clear();
}

void TextTemplateStore::createSampleFileIfNeeded() {
  if (path_.empty() || sample_.empty()) {
    return;
  }
  if (createFileIfAbsent(path_, sample_)) {
    // Read the new file at the next lookup.
    checked_ = false;
  }
}

const std::vector<TextTemplateEntry>& TextTemplateStore::entries() {
  double now = now_();
  if (checked_ && now - lastChecked_ < kRecheckInterval) {
    return entries_;
  }
  checked_ = true;
  lastChecked_ = now;
  FileStamp stamp = stampOf(path_);
  if (stamp == stamp_) {
    return entries_;
  }
  stamp_ = stamp;
  std::optional<std::string> content;
  if (stamp.exists) {
    content = readWholeFile(path_);
  }
  entries_ = content ? ParseTextTemplates(*content)
                     : std::vector<TextTemplateEntry>();
  return entries_;
}

}  // namespace McBopomofo::SmartLearning
