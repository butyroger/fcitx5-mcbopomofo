#include "UserPhraseFile.h"

#include <optional>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "AtomicFile.h"

namespace McBopomofo::SmartLearning::UserPhraseFile {
namespace fs = std::filesystem;

namespace {

// White space and line breaks: the Unicode separators (Zs, Zl, Zp), the tab,
// U+000A to U+000D and U+0085.
bool isWhiteSpace(char32_t c) {
  return (c >= 0x09 && c <= 0x0D) || c == 0x20 || c == 0x85 || c == 0xA0 ||
         c == 0x1680 || (c >= 0x2000 && c <= 0x200A) || c == 0x2028 ||
         c == 0x2029 || c == 0x202F || c == 0x205F || c == 0x3000;
}

// Decodes the UTF-8 sequence at |s[pos]|, giving its length in |length|. An
// invalid sequence decodes as one byte that is not white space.
char32_t decodeAt(std::string_view s, size_t pos, size_t* length) {
  auto byte = [&](size_t i) { return static_cast<unsigned char>(s[i]); };
  unsigned char lead = byte(pos);
  size_t count = 0;
  char32_t c = 0;
  if (lead < 0x80) {
    *length = 1;
    return lead;
  }
  if ((lead & 0xE0) == 0xC0) {
    count = 2;
    c = lead & 0x1F;
  } else if ((lead & 0xF0) == 0xE0) {
    count = 3;
    c = lead & 0x0F;
  } else if ((lead & 0xF8) == 0xF0) {
    count = 4;
    c = lead & 0x07;
  } else {
    *length = 1;
    return 0xFFFD;
  }
  if (pos + count > s.size()) {
    *length = 1;
    return 0xFFFD;
  }
  for (size_t i = 1; i < count; ++i) {
    if ((byte(pos + i) & 0xC0) != 0x80) {
      *length = 1;
      return 0xFFFD;
    }
    c = (c << 6) | (byte(pos + i) & 0x3F);
  }
  *length = count;
  return c;
}

std::string_view trimmed(std::string_view s) {
  size_t start = 0;
  while (start < s.size()) {
    size_t length = 0;
    if (!isWhiteSpace(decodeAt(s, start, &length))) {
      break;
    }
    start += length;
  }
  size_t end = s.size();
  while (end > start) {
    // The start of the last character.
    size_t last = end - 1;
    while (last > start &&
           (static_cast<unsigned char>(s[last]) & 0xC0) == 0x80) {
      --last;
    }
    size_t length = 0;
    char32_t c = decodeAt(s, last, &length);
    if (last + length != end) {
      // Not one whole character: an invalid sequence.
      break;
    }
    if (!isWhiteSpace(c)) {
      break;
    }
    end = last;
  }
  return s.substr(start, end - start);
}

// The value and the reading of a line holding a phrase.
std::optional<std::pair<std::string_view, std::string_view>> phraseOfLine(
    std::string_view line) {
  std::string_view t = trimmed(line);
  size_t space = t.find(' ');
  if (space == std::string_view::npos ||
      t.find(' ', space + 1) != std::string_view::npos) {
    return std::nullopt;
  }
  return std::make_pair(t.substr(0, space), t.substr(space + 1));
}

// Split on '\n', keeping the empty part after a final line break.
std::vector<std::string_view> splitLines(std::string_view content) {
  std::vector<std::string_view> lines;
  size_t start = 0;
  while (true) {
    size_t end = content.find('\n', start);
    if (end == std::string_view::npos) {
      lines.push_back(content.substr(start));
      return lines;
    }
    lines.push_back(content.substr(start, end - start));
    start = end + 1;
  }
}

std::string joinLines(const std::vector<std::string_view>& lines) {
  std::string result;
  for (size_t i = 0; i < lines.size(); ++i) {
    if (i > 0) {
      result += '\n';
    }
    result += lines[i];
  }
  return result;
}

bool isPlainPart(const std::string& s) {
  return !s.empty() && s.find_first_of(" \t\r\n") == std::string::npos;
}

bool holdsPhrase(std::string_view content, std::string_view value,
                 std::string_view reading) {
  for (std::string_view line : splitLines(content)) {
    auto phrase = phraseOfLine(line);
    if (phrase && phrase->first == value && phrase->second == reading) {
      return true;
    }
  }
  return false;
}

// The content without the lines of the phrase, or nullopt if it has none.
std::optional<std::string> withoutPhrase(std::string_view content,
                                         std::string_view value,
                                         std::string_view reading) {
  std::vector<std::string_view> kept;
  bool removed = false;
  for (std::string_view line : splitLines(content)) {
    auto phrase = phraseOfLine(line);
    if (phrase && phrase->first == value && phrase->second == reading) {
      removed = true;
      continue;
    }
    kept.push_back(line);
  }
  if (!removed) {
    return std::nullopt;
  }
  return joinLines(kept);
}

std::string withPhraseAtEnd(std::string content, const std::string& value,
                            const std::string& reading) {
  if (!content.empty() && content.back() != '\n') {
    content += '\n';
  }
  content += value;
  content += ' ';
  content += reading;
  content += '\n';
  return content;
}

bool isMissing(const fs::path& path) {
  std::error_code ec;
  return fs::symlink_status(path, ec).type() == fs::file_type::not_found;
}

// Takes the phrase out of a file that may be missing. A file that cannot be
// read is left as it is.
void removePhraseFromFile(const fs::path& path, const std::string& value,
                          const std::string& reading) {
  if (isMissing(path)) {
    return;
  }
  auto content = readWholeFile(path);
  if (!content) {
    return;
  }
  if (auto edited = withoutPhrase(*content, value, reading)) {
    writeFileAtomically(path, *edited);
  }
}

// The content with the phrase learned, or nullopt if it is already the first
// phrase of its reading.
std::optional<std::string> learnedContent(std::string content,
                                          const std::string& value,
                                          const std::string& reading) {
  std::vector<std::string_view> lines = splitLines(content);
  std::optional<size_t> firstOfReading;
  std::vector<bool> isThePhrase(lines.size(), false);
  for (size_t i = 0; i < lines.size(); ++i) {
    if (trimmed(lines[i]).starts_with('#')) {
      continue;
    }
    auto phrase = phraseOfLine(lines[i]);
    if (!phrase || phrase->second != reading) {
      continue;
    }
    if (!firstOfReading) {
      firstOfReading = i;
    }
    isThePhrase[i] = phrase->first == value;
  }

  if (!firstOfReading) {
    return withPhraseAtEnd(std::move(content), value, reading);
  }
  if (isThePhrase[*firstOfReading]) {
    // Already the first one.
    return std::nullopt;
  }
  const std::string newLine = value + " " + reading;
  std::vector<std::string_view> edited;
  edited.reserve(lines.size() + 1);
  for (size_t i = 0; i < lines.size(); ++i) {
    if (i == *firstOfReading) {
      edited.push_back(newLine);
    }
    if (!isThePhrase[i]) {
      edited.push_back(lines[i]);
    }
  }
  return joinLines(edited);
}

}  // namespace

bool fitsInColumn(std::string_view text) {
  if (text.empty()) {
    return false;
  }
  for (size_t pos = 0; pos < text.size();) {
    size_t length = 0;
    if (isWhiteSpace(decodeAt(text, pos, &length))) {
      return false;
    }
    pos += length;
  }
  return true;
}

bool learn(const fs::path& data, const fs::path& exclude,
           const std::string& value, const std::string& reading) {
  return !learn(data, exclude, {{value, reading}}).empty();
}

std::vector<std::pair<std::string, std::string>> learn(
    const fs::path& data, const fs::path& exclude,
    const std::vector<std::pair<std::string, std::string>>& phrases) {
  if (phrases.empty()) {
    return {};
  }
  // Never bring back a phrase the user excluded.
  std::optional<std::string> excluded;
  if (!isMissing(exclude)) {
    excluded = readWholeFile(exclude);
    if (!excluded) {
      return {};
    }
  }
  auto content = readWholeFile(data);
  if (!content) {
    return {};
  }
  std::vector<std::pair<std::string, std::string>> learned;
  for (const auto& [value, reading] : phrases) {
    if (!isPlainPart(value) || !isPlainPart(reading) ||
        (excluded && holdsPhrase(*excluded, value, reading))) {
      continue;
    }
    if (auto edited = learnedContent(*content, value, reading)) {
      content = std::move(edited);
      learned.emplace_back(value, reading);
    }
  }
  if (learned.empty() || !writeFileAtomically(data, *content)) {
    return {};
  }
  return learned;
}

bool write(const fs::path& data, const fs::path& exclude,
           const std::string& value, const std::string& reading) {
  if (!isPlainPart(value) || !isPlainPart(reading)) {
    return false;
  }
  auto content = readWholeFile(data);
  if (!content) {
    return false;
  }
  removePhraseFromFile(exclude, value, reading);
  if (holdsPhrase(*content, value, reading)) {
    return false;
  }
  return writeFileAtomically(
      data, withPhraseAtEnd(std::move(*content), value, reading));
}

bool remove(const fs::path& data, const fs::path& exclude,
            const std::string& value, const std::string& reading) {
  if (!isPlainPart(value) || !isPlainPart(reading)) {
    return false;
  }
  auto content = readWholeFile(exclude);
  if (!content) {
    return false;
  }
  removePhraseFromFile(data, value, reading);
  if (holdsPhrase(*content, value, reading)) {
    return false;
  }
  return writeFileAtomically(
      exclude, withPhraseAtEnd(std::move(*content), value, reading));
}

}  // namespace McBopomofo::SmartLearning::UserPhraseFile
