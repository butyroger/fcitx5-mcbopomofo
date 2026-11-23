#include "EnglishWords.h"

#include <algorithm>
#include <charconv>
#include <set>
#include <system_error>
#include <unordered_map>
#include <utility>

#include "AtomicFile.h"
#include "LearningPaths.h"

namespace McBopomofo::SmartLearning {
namespace fs = std::filesystem;

namespace {

constexpr char kEnglishFolderName[] = "english";
constexpr char kRemovedFilename[] = "removed.txt";
constexpr std::string_view kRemovedEnglishKind = "english";

// Well-known words with uppercase letters after the first one, whose case is
// fixed even if they have never been typed.
constexpr const char* kWellKnownMixedCaseWords[] = {
    "iPhone",      "iPad",        "iPod",       "iMac",       "iCloud",
    "iMessage",    "iTunes",      "iOS",        "iPadOS",     "macOS",
    "watchOS",     "tvOS",        "visionOS",   "AirPods",    "AirDrop",
    "AirPlay",     "AirTag",      "MacBook",    "FaceTime",   "AppleCare",
    "HomePod",     "CarPlay",     "OpenAI",     "ChatGPT",    "DeepSeek",
    "GitHub",      "GitLab",      "YouTube",    "LinkedIn",   "PayPal",
    "WhatsApp",    "TikTok",      "WeChat",     "PowerPoint", "OneDrive",
    "OneNote",     "SharePoint",  "FileMaker",  "JavaScript", "TypeScript",
    "PostgreSQL",  "MySQL",       "MariaDB",    "SQLite",     "MongoDB",
    "WordPress",   "WooCommerce", "PlayStation", "SmartThings", "DevOps",
    "eBay",        "eSIM",        "McDonald",   "LaTeX",      "PhD",
    "VoIP",
};

bool isAsciiUpper(char c) { return c >= 'A' && c <= 'Z'; }
bool isAsciiLower(char c) { return c >= 'a' && c <= 'z'; }
bool isAsciiLetter(char c) { return isAsciiUpper(c) || isAsciiLower(c); }

std::string asciiLowercased(std::string_view s) {
  std::string result(s);
  for (char& c : result) {
    if (isAsciiUpper(c)) {
      c = static_cast<char>(c - 'A' + 'a');
    }
  }
  return result;
}

std::string asciiUppercased(std::string_view s) {
  std::string result(s);
  for (char& c : result) {
    if (isAsciiLower(c)) {
      c = static_cast<char>(c - 'a' + 'A');
    }
  }
  return result;
}

// The number of characters (code points) of UTF-8 text.
size_t characterCount(std::string_view s) {
  size_t count = 0;
  for (char c : s) {
    if ((static_cast<unsigned char>(c) & 0xC0) != 0x80) {
      ++count;
    }
  }
  return count;
}

bool allAsciiLetters(std::string_view s) {
  return std::all_of(s.begin(), s.end(), isAsciiLetter);
}

const std::unordered_map<std::string, std::string>& wellKnownMixedCaseWords() {
  static const auto* words = [] {
    auto* result = new std::unordered_map<std::string, std::string>();
    for (const char* word : kWellKnownMixedCaseWords) {
      (*result)[asciiLowercased(word)] = word;
    }
    return result;
  }();
  return *words;
}

// Whether a word has an uppercase letter after the first one, but is not all
// uppercase, like iPhone and OpenAI, but not Apple or NASA.
bool isMixedCase(const std::string& word) {
  return word.size() > 1 &&
         std::any_of(word.begin() + 1, word.end(), isAsciiUpper) &&
         word != asciiUppercased(word);
}

// Splits on |separator|. Empty parts are dropped unless |keepEmpty|.
std::vector<std::string_view> split(std::string_view s, char separator,
                                    bool keepEmpty) {
  std::vector<std::string_view> parts;
  size_t start = 0;
  while (true) {
    size_t end = s.find(separator, start);
    std::string_view part = s.substr(
        start, end == std::string_view::npos ? std::string_view::npos
                                             : end - start);
    if (keepEmpty || !part.empty()) {
      parts.push_back(part);
    }
    if (end == std::string_view::npos) {
      break;
    }
    start = end + 1;
  }
  return parts;
}

// An optional sign and decimal digits, with nothing else.
std::optional<long long> parseInteger(std::string_view s) {
  if (!s.empty() && s.front() == '+') {
    s.remove_prefix(1);
    if (!s.empty() && s.front() == '-') {
      return std::nullopt;
    }
  }
  long long value = 0;
  auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), value);
  if (s.empty() || ec != std::errc() || ptr != s.data() + s.size()) {
    return std::nullopt;
  }
  return value;
}

// A decimal number, with nothing else.
std::optional<double> parseNumber(std::string_view s) {
  if (!s.empty() && s.front() == '+') {
    s.remove_prefix(1);
    if (!s.empty() && s.front() == '-') {
      return std::nullopt;
    }
  }
  double value = 0;
  auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), value);
  if (s.empty() || ec != std::errc() || ptr != s.data() + s.size()) {
    return std::nullopt;
  }
  return value;
}

// Each line of a file is a word, a tab, and the count.
std::map<std::string, long long> parseCounts(std::string_view content) {
  std::map<std::string, long long> result;
  for (std::string_view line : split(content, '\n', /*keepEmpty=*/false)) {
    auto parts = split(line, '\t', /*keepEmpty=*/false);
    if (parts.size() != 2) {
      continue;
    }
    if (auto count = parseInteger(parts[1])) {
      result[std::string(parts[0])] = *count;
    }
  }
  return result;
}

// The English words in the removed file: each line is the kind, the key and
// the time it was removed. A time of 0 means the entry is not removed.
std::set<std::string> removedEnglishWords(const fs::path& path) {
  std::map<std::string, double> times;
  if (auto content = readWholeFile(path)) {
    for (std::string_view line : split(*content, '\n', /*keepEmpty=*/false)) {
      auto parts = split(line, '\t', /*keepEmpty=*/true);
      if (parts.size() != 3 || parts[0] != kRemovedEnglishKind) {
        continue;
      }
      if (auto time = parseNumber(parts[2])) {
        times[std::string(parts[1])] = *time;
      }
    }
  }
  std::set<std::string> result;
  for (const auto& [word, time] : times) {
    if (time != 0) {
      result.insert(word);
    }
  }
  return result;
}

// "a/./b/" and "a/b" are the same folder.
fs::path normalizedFolder(const fs::path& path) {
  fs::path normal = path.lexically_normal();
  if (normal.filename().empty() && normal.has_parent_path()) {
    normal = normal.parent_path();
  }
  return normal;
}

std::shared_ptr<EnglishLexicon> emptyLexicon() {
  return EnglishLexicon::fromWords({});
}

}  // namespace

// MARK: - EnglishLexicon

std::shared_ptr<EnglishLexicon> EnglishLexicon::loadFromFile(
    const fs::path& path) {
  auto content = readWholeFile(path);
  if (!content) {
    return emptyLexicon();
  }
  std::vector<std::string> words;
  words.reserve(content->size() / 8);
  std::string_view rest(*content);
  while (!rest.empty()) {
    size_t end = rest.find('\n');
    std::string_view line = rest.substr(0, end);
    rest = end == std::string_view::npos ? std::string_view()
                                         : rest.substr(end + 1);
    if (!line.empty() && line.back() == '\r') {
      line.remove_suffix(1);
    }
    if (!line.empty() && allAsciiLetters(line)) {
      words.push_back(asciiLowercased(line));
    }
  }
  std::sort(words.begin(), words.end());
  words.erase(std::unique(words.begin(), words.end()), words.end());
  words.shrink_to_fit();
  return std::shared_ptr<EnglishLexicon>(new EnglishLexicon(std::move(words)));
}

std::shared_ptr<EnglishLexicon> EnglishLexicon::fromWords(
    std::vector<std::string> words) {
  std::vector<std::string> kept;
  kept.reserve(words.size());
  for (auto& word : words) {
    if (!word.empty() && allAsciiLetters(word)) {
      kept.push_back(asciiLowercased(word));
    }
  }
  std::sort(kept.begin(), kept.end());
  kept.erase(std::unique(kept.begin(), kept.end()), kept.end());
  return std::shared_ptr<EnglishLexicon>(new EnglishLexicon(std::move(kept)));
}

bool EnglishLexicon::contains(std::string_view lowercased) const {
  return std::binary_search(words_.begin(), words_.end(), lowercased);
}

bool EnglishLexicon::hasLongerWordWithPrefix(
    std::string_view lowercased) const {
  auto it = std::lower_bound(words_.begin(), words_.end(), lowercased);
  if (it != words_.end() && *it == lowercased) {
    ++it;
  }
  // The words are sorted and unique, so the next one is longer if it starts
  // with the prefix.
  return it != words_.end() && it->starts_with(lowercased);
}

// MARK: - EnglishWordStore

EnglishWordStore::EnglishWordStore(
    std::string deviceID, std::string deviceName,
    std::shared_ptr<const EnglishLexicon> lexicon, std::function<double()> now)
    : deviceID_(std::move(deviceID)),
      ownFilename_(learningFilename(deviceID_, sanitizeDeviceName(deviceName))),
      lexicon_(lexicon ? std::move(lexicon) : emptyLexicon()),
      now_(std::move(now)) {}

void EnglishWordStore::setLearningFolder(fs::path learningFolder) {
  // A relative path would resolve against the process's working directory.
  if (learningFolder.empty() || !learningFolder.is_absolute()) {
    return;
  }
  if (hasFolder_ &&
      normalizedFolder(learningFolder) == normalizedFolder(learningFolder_)) {
    // The own file is already loaded (or still pending) from this folder;
    // loading it again would add its counts a second time.
    reload();
    return;
  }
  if (hasFolder_) {
    // The own words belong to the old folder: keep them there, then start
    // over with what the new folder holds.
    if (!localFilePending_ && unsavedChanges_) {
      save();
    }
    localCounts_.clear();
    unsavedChanges_ = false;
  }
  learningFolder_ = std::move(learningFolder);
  hasFolder_ = true;
  localFilePending_ = true;
  reload();
}

void EnglishWordStore::record(const std::vector<std::string>& words) {
  loadIfNeeded();
  for (const auto& word : words) {
    // A tab or a line break would break the line format of the file.
    if (characterCount(word) < kMinimumLength ||
        word.find_first_of("\t\n") != std::string::npos) {
      continue;
    }
    localCounts_[word] += 1;
    counts_[word] += 1;
    unsavedChanges_ = true;
  }
}

bool EnglishWordStore::save() {
  if (!hasFolder_) {
    return false;
  }
  // Never overwrite the own file before it has been loaded.
  if (localFilePending_) {
    if (!loadLocalFileIfNeeded()) {
      return false;
    }
    reload();
  }

  std::vector<std::string> lines;
  lines.reserve(localCounts_.size());
  for (const auto& [word, count] : localCounts_) {
    lines.push_back(word + "\t" + std::to_string(count) + "\n");
  }
  std::sort(lines.begin(), lines.end());
  std::string content;
  for (const auto& line : lines) {
    content += line;
  }

  // Create learning/english/ when it is missing, but never the user data
  // folder itself: a missing one may be a sync folder that is not mounted.
  if (learningFolderState(englishFolder(), 2) ==
      LearningFolderState::kUnavailable) {
    return false;
  }
  std::error_code ec;
  fs::create_directories(englishFolder(), ec);
  if (ec) {
    return false;
  }
  if (!writeFileAtomically(ownFilePath(), content)) {
    return false;
  }
  unsavedChanges_ = false;
  return true;
}

std::optional<std::string> EnglishWordStore::preferredCase(
    const std::string& word) {
  if (characterCount(word) < 2 || word != asciiLowercased(word)) {
    return std::nullopt;
  }
  loadIfNeeded();
  // A word typed only once may be a typo. The most typed wins; on a tie, the
  // smallest word.
  const std::string* best = nullptr;
  long long bestCount = 0;
  for (const auto& [key, count] : counts_) {
    if (count >= 2 && asciiLowercased(key) == word && isMixedCase(key) &&
        (best == nullptr || count > bestCount)) {
      best = &key;
      bestCount = count;
    }
  }
  if (best != nullptr) {
    return *best;
  }
  const auto& wellKnown = wellKnownMixedCaseWords();
  auto it = wellKnown.find(word);
  if (it != wellKnown.end()) {
    return it->second;
  }
  return std::nullopt;
}

bool EnglishWordStore::isWord(const std::string& word) {
  if (word.size() < 2 || !allAsciiLetters(word)) {
    return false;
  }
  size_t trailingUppercase = 0;
  for (auto it = word.rbegin(); it != word.rend() && isAsciiUpper(*it); ++it) {
    ++trailingUppercase;
  }
  if (trailingUppercase == word.size() || trailingUppercase >= 2) {
    return true;
  }
  const std::string lowercased = asciiLowercased(word);
  if (wellKnownMixedCaseWords().count(lowercased) > 0) {
    return true;
  }
  loadIfNeeded();
  for (const auto& [key, count] : counts_) {
    if (count >= 2 && asciiLowercased(key) == lowercased) {
      return true;
    }
  }
  return lexicon_->contains(lowercased);
}

bool EnglishWordStore::isWordOrPrefix(const std::string& text) {
  if (isWord(text)) {
    return true;
  }
  const std::string lowercased = asciiLowercased(text);
  loadIfNeeded();
  for (const auto& [key, count] : counts_) {
    if (count >= 2 && asciiLowercased(key).starts_with(lowercased)) {
      return true;
    }
  }
  // An empty text has no completions.
  return !lowercased.empty() && lexicon_->hasLongerWordWithPrefix(lowercased);
}

void EnglishWordStore::loadIfNeeded() {
  if (loaded_ && now_() - lastLoaded_ < kReloadInterval) {
    return;
  }
  reload();
}

void EnglishWordStore::reload() {
  loadLocalFileIfNeeded();
  std::map<std::string, long long> merged = localCounts_;
  if (hasFolder_) {
    for (const auto& path :
         otherDevicesLearningFiles(englishFolder(), ownFilename_)) {
      if (auto content = readWholeFile(path)) {
        for (const auto& [word, count] : parseCounts(*content)) {
          merged[word] += count;
        }
      }
    }
    // Words removed by the user, maybe on another device.
    for (const auto& word :
         removedEnglishWords(learningFolder_ / kRemovedFilename)) {
      merged.erase(word);
    }
  }
  counts_ = std::move(merged);
  loaded_ = true;
  lastLoaded_ = now_();
}

bool EnglishWordStore::loadLocalFileIfNeeded() {
  if (!localFilePending_ || !hasFolder_) {
    return false;
  }
  const fs::path folder = englishFolder();
  // learning/english/ and learning/ may both be missing.
  switch (learningFolderState(folder, 2)) {
    case LearningFolderState::kUnavailable:
      // The user data folder may not have been synced yet.
      return false;
    case LearningFolderState::kCreatable:
      // No english folder yet, so no own file: start empty. Saving creates
      // the folder.
      localFilePending_ = false;
      return true;
    case LearningFolderState::kExists:
      break;
  }

  std::error_code ec;

  const fs::path path = ownFilePath();
  if (!fs::exists(fs::symlink_status(path, ec))) {
    // The computer may have been renamed since the file was written.
    if (auto renamed = findRenamedOwnFile(folder, deviceID_, ownFilename_)) {
      std::error_code renameEc;
      fs::rename(*renamed, path, renameEc);
      if (renameEc) {
        // Starting a new file would split this device's words in two.
        return false;
      }
    }
  }

  if (fs::exists(fs::symlink_status(path, ec))) {
    auto content = readWholeFile(path);
    if (!content) {
      // Never replace a file that could not be read.
      return false;
    }
    // Adding keeps what was recorded while the file was not available.
    for (const auto& [word, count] : parseCounts(*content)) {
      localCounts_[word] += count;
    }
  }
  localFilePending_ = false;
  return true;
}

fs::path EnglishWordStore::englishFolder() const {
  return learningFolder_ / kEnglishFolderName;
}

fs::path EnglishWordStore::ownFilePath() const {
  return englishFolder() / ownFilename_;
}

}  // namespace McBopomofo::SmartLearning
