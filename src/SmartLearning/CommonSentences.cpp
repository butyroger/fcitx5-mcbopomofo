#include "CommonSentences.h"

#include <unicode/ubrk.h>
#include <unicode/uchar.h>
#include <unicode/utext.h>
#include <unicode/utf8.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <system_error>
#include <utility>

#include "AtomicFile.h"
#include "LearningFileUtils.h"
#include "LearningPaths.h"

namespace McBopomofo::SmartLearning {
namespace fs = std::filesystem;

namespace {

constexpr char kSentencesFolderName[] = "sentences";
constexpr double kDay = 86400;

// The code point at |*index|, moving the index past it; -1 if it is not
// valid UTF-8.
UChar32 nextCodePoint(std::string_view s, size_t* index) {
  UChar32 c = 0;
  int32_t i = static_cast<int32_t>(*index);
  U8_NEXT(s.data(), i, static_cast<int32_t>(s.size()), c);
  *index = static_cast<size_t>(i);
  return c;
}

// Punctuation, symbols (emoji among them), line breaks and tabs.
bool isSeparator(UChar32 c) {
  if (c < 0) {
    return false;
  }
  if (c == '\t' || (c >= 0x0A && c <= 0x0D) || c == 0x85 || c == 0x2028 ||
      c == 0x2029) {
    return true;
  }
  return (U_GET_GC_MASK(c) & (U_GC_P_MASK | U_GC_S_MASK)) != 0;
}

// The spaces trimmed from both ends of a sentence: space separators and tab.
bool isTrimmedSpace(UChar32 c) {
  return c == '\t' || (c >= 0 && u_charType(c) == U_SPACE_SEPARATOR);
}

bool isWhitespace(UChar32 c) {
  return c >= 0 && u_hasBinaryProperty(c, UCHAR_WHITE_SPACE);
}

// A character break iterator, opened once.
class CharacterBreaks {
 public:
  CharacterBreaks() {
    UErrorCode status = U_ZERO_ERROR;
    iterator_ = ubrk_open(UBRK_CHARACTER, "", nullptr, 0, &status);
    if (U_FAILURE(status)) {
      iterator_ = nullptr;
    }
  }
  ~CharacterBreaks() {
    if (iterator_ != nullptr) {
      ubrk_close(iterator_);
    }
  }
  CharacterBreaks(const CharacterBreaks&) = delete;
  CharacterBreaks& operator=(const CharacterBreaks&) = delete;

  // The byte offsets where the characters (grapheme clusters) start, and the
  // size of the text at the end.
  std::vector<size_t> boundaries(std::string_view s) {
    std::vector<size_t> result;
    UErrorCode status = U_ZERO_ERROR;
    UText* text = nullptr;
    if (iterator_ != nullptr) {
      text = utext_openUTF8(nullptr, s.data(), static_cast<int64_t>(s.size()),
                            &status);
      if (U_SUCCESS(status)) {
        ubrk_setUText(iterator_, text, &status);
      }
    }
    if (iterator_ != nullptr && U_SUCCESS(status)) {
      for (int32_t b = ubrk_first(iterator_); b != UBRK_DONE;
           b = ubrk_next(iterator_)) {
        result.push_back(static_cast<size_t>(b));
      }
    } else {
      // Code points, if the iterator cannot be used.
      result.push_back(0);
      for (size_t i = 0; i < s.size();) {
        nextCodePoint(s, &i);
        result.push_back(i);
      }
    }
    if (text != nullptr) {
      utext_close(text);
    }
    if (result.empty() || result.back() != s.size()) {
      result.push_back(s.size());
    }
    return result;
  }

 private:
  UBreakIterator* iterator_ = nullptr;
};

std::vector<size_t> characterBoundaries(std::string_view s) {
  thread_local CharacterBreaks breaks;
  return breaks.boundaries(s);
}

// Splits at the separators, keeping empty parts.
std::vector<std::string_view> splitAtSeparators(std::string_view s) {
  std::vector<std::string_view> parts;
  size_t start = 0;
  for (size_t i = 0; i < s.size();) {
    size_t begin = i;
    UChar32 c = nextCodePoint(s, &i);
    if (isSeparator(c)) {
      parts.push_back(s.substr(start, begin - start));
      start = i;
    }
  }
  parts.push_back(s.substr(start));
  return parts;
}

std::string_view trimSpaces(std::string_view s) {
  // From the front.
  size_t i = 0;
  while (i < s.size()) {
    size_t next = i;
    if (!isTrimmedSpace(nextCodePoint(s, &next))) {
      break;
    }
    i = next;
  }
  s.remove_prefix(i);
  // From the back: find the end of the last code point that is not a space.
  size_t end = 0;
  for (size_t j = 0; j < s.size();) {
    UChar32 c = nextCodePoint(s, &j);
    if (!isTrimmedSpace(c)) {
      end = j;
    }
  }
  return s.substr(0, end);
}

// Each line is the sentence, the count, the time last used, and 1 if it has
// been added by the user.
std::map<std::string, CommonSentenceStore::Entry> parseEntries(
    std::string_view content) {
  std::map<std::string, CommonSentenceStore::Entry> result;
  for (std::string_view line : split(content, '\n', /*keepEmpty=*/false)) {
    auto parts = split(line, '\t', /*keepEmpty=*/true);
    if (parts.size() < 3) {
      continue;
    }
    auto count = parseInteger(parts[1]);
    auto time = parseNumber(parts[2]);
    if (!count || !time) {
      continue;
    }
    result[std::string(parts[0])] = CommonSentenceStore::Entry{
        *count, *time, parts.size() >= 4 && parts[3] == "1"};
  }
  return result;
}

// Adds the counts, keeps the later time, and stays added by the user.
void combine(CommonSentenceStore::Entry* into,
             const CommonSentenceStore::Entry& entry) {
  into->count += entry.count;
  into->lastUsed = std::max(into->lastUsed, entry.lastUsed);
  into->pinned = into->pinned || entry.pinned;
}

}  // namespace

size_t SentenceLength(std::string_view sentence) {
  size_t count = 0;
  bool inWord = false;
  auto boundaries = characterBoundaries(sentence);
  for (size_t i = 0; i + 1 < boundaries.size(); i++) {
    std::string_view character =
        sentence.substr(boundaries[i], boundaries[i + 1] - boundaries[i]);
    if (character.size() == 1 &&
        isalnum(static_cast<unsigned char>(character[0])) &&
        static_cast<unsigned char>(character[0]) < 0x80) {
      if (!inWord) {
        count++;
        inWord = true;
      }
      continue;
    }
    inWord = false;
    size_t index = 0;
    if (!isWhitespace(nextCodePoint(character, &index))) {
      count++;
    }
  }
  return count;
}

bool IsValidSentence(std::string_view sentence) {
  size_t length = SentenceLength(sentence);
  if (length < CommonSentenceStore::kMinimumLength ||
      length > CommonSentenceStore::kMaximumLength) {
    return false;
  }
  for (size_t i = 0; i < sentence.size();) {
    if (isSeparator(nextCodePoint(sentence, &i))) {
      return false;
    }
  }
  return true;
}

std::vector<std::string> SplitSentences(std::string_view committed) {
  std::vector<std::string> sentences;
  for (std::string_view part : splitAtSeparators(committed)) {
    std::string_view trimmed = trimSpaces(part);
    if (IsValidSentence(trimmed)) {
      sentences.emplace_back(trimmed);
    }
  }
  return sentences;
}

double CommonSentenceStore::lifetime(const Entry& entry) {
  long long extra = std::max(0LL, entry.count - kThreshold) / 3;
  return 90 * kDay + static_cast<double>(extra) * 30 * kDay;
}

CommonSentenceStore::CommonSentenceStore(
    std::string deviceID, std::string deviceName,
    std::function<double()> wallNow,
    std::shared_ptr<RemovedEntries> removedEntries)
    : deviceID_(std::move(deviceID)),
      ownFilename_(learningFilename(deviceID_, sanitizeDeviceName(deviceName))),
      wallNow_(std::move(wallNow)),
      removedEntries_(removedEntries
                          ? std::move(removedEntries)
                          : std::make_shared<RemovedEntries>(steadySeconds)) {}

void CommonSentenceStore::setLearningFolder(fs::path learningFolder) {
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
    // The own sentences belong to the old folder: keep them there, then
    // start over with what the new folder holds.
    if (!localFilePending_ && unsavedChanges_) {
      save();
    }
    // While the old folder's own file is pending, what was recorded meanwhile
    // is dropped: that file was never read, so writing it is unsafe.
    localEntries_.clear();
    unsavedChanges_ = false;
  }
  learningFolder_ = std::move(learningFolder);
  hasFolder_ = true;
  localFilePending_ = true;
  removedEntries_->setLearningFolder(learningFolder_);
  reload();
}

void CommonSentenceStore::record(std::string_view committed) {
  if (!enabled_) {
    return;
  }
  auto sentences = SplitSentences(committed);
  if (sentences.empty()) {
    return;
  }
  const double now = wallNow_();
  for (const auto& sentence : sentences) {
    for (auto* entries : {&localEntries_, &mergedEntries_}) {
      Entry& entry = (*entries)[sentence];
      entry.count += 1;
      entry.lastUsed = now;
    }
  }
  unsavedChanges_ = true;
}

std::optional<std::string> CommonSentenceStore::completion(
    std::string_view typedHead) {
  if (!enabled_) {
    return std::nullopt;
  }
  // What has been typed since the last separator, without leading spaces.
  std::string_view prefix = typedHead;
  for (size_t i = 0; i < typedHead.size();) {
    if (isSeparator(nextCodePoint(typedHead, &i))) {
      prefix = typedHead.substr(i);
    }
  }
  auto prefixBoundaries = characterBoundaries(prefix);
  size_t skipped = 0;
  for (size_t i = 0; i + 1 < prefixBoundaries.size(); i++) {
    size_t index = prefixBoundaries[i];
    if (!isWhitespace(nextCodePoint(prefix, &index))) {
      break;
    }
    skipped = i + 1;
  }
  prefix.remove_prefix(prefixBoundaries[skipped]);
  if (prefix.empty() ||
      prefixBoundaries.size() - 1 - skipped < kMinimumPrefixLength) {
    return std::nullopt;
  }

  const double now = wallNow_();
  const std::string key(prefix);
  const std::string* best = nullptr;
  const Entry* bestEntry = nullptr;
  for (auto it = mergedEntries_.lower_bound(key);
       it != mergedEntries_.end() && it->first.compare(0, key.size(), key) == 0;
       ++it) {
    const auto& [sentence, entry] = *it;
    if (sentence.size() == key.size() ||
        !(entry.pinned || entry.count >= kThreshold)) {
      continue;
    }
    // The prefix must end between two characters of the sentence.
    auto boundaries = characterBoundaries(sentence);
    if (!std::binary_search(boundaries.begin(), boundaries.end(),
                            key.size())) {
      continue;
    }
    if (!isAlive(sentence, entry, now)) {
      continue;
    }
    if (bestEntry == nullptr || entry.count > bestEntry->count ||
        (entry.count == bestEntry->count &&
         entry.lastUsed > bestEntry->lastUsed)) {
      best = &sentence;
      bestEntry = &entry;
    }
  }
  if (best == nullptr) {
    return std::nullopt;
  }
  return best->substr(key.size());
}

void CommonSentenceStore::reload() {
  loadLocalFileIfNeeded();
  if (hasFolder_) {
    // Another program may have removed or added a sentence.
    removedEntries_->reloadIfChanged();
  }
  mergeLocalFileIfChanged();
  dropRemovedLocalSentences();
  merge();
}

void CommonSentenceStore::reloadIfNeeded() {
  const double now = wallNow_();
  // A clock set back also reloads.
  if (now >= lastLoaded_ && now - lastLoaded_ < kReloadInterval) {
    return;
  }
  if (!localFilePending_ && takeSnapshot() == snapshot_ &&
      stampOf(ownFilePath()) == localStamp_) {
    // Nothing to read again: the removed entries are looked up as the
    // sentences are used.
    lastLoaded_ = now;
    return;
  }
  reload();
}

CommonSentenceStore::Snapshot CommonSentenceStore::takeSnapshot() const {
  if (!hasFolder_) {
    return {};
  }
  return stampsOf(otherDevicesLearningFiles(sentencesFolder(), ownFilename_));
}

bool CommonSentenceStore::save() {
  if (!hasFolder_) {
    return false;
  }
  // Never overwrite the own file before it has been loaded.
  if (localFilePending_) {
    if (!loadLocalFileIfNeeded()) {
      return false;
    }
    merge();
  }
  // Nor what another program wrote to it since.
  switch (mergeLocalFileIfChanged()) {
    case FileChange::kUnreadable:
      return false;
    case FileChange::kChanged:
      removedEntries_->reloadIfChanged();
      dropRemovedLocalSentences();
      merge();
      break;
    case FileChange::kUnchanged:
      break;
  }

  const double now = wallNow_();
  std::vector<std::string> lines;
  for (const auto& [sentence, entry] : localEntries_) {
    // Forget the sentences that have not been used for a long time.
    if (!entry.pinned && now - entry.lastUsed > lifetime(entry)) {
      continue;
    }
    lines.push_back(sentence + "\t" + std::to_string(entry.count) + "\t" +
                    std::to_string(static_cast<long long>(entry.lastUsed)) +
                    "\t" + (entry.pinned ? "1" : "0") + "\n");
  }
  std::sort(lines.begin(), lines.end());
  std::string content;
  for (const auto& line : lines) {
    content += line;
  }

  // Create learning/sentences/ when it is missing, but never the user data
  // folder itself: a missing one may be a sync folder that is not mounted.
  const fs::path folder = sentencesFolder();
  if (learningFolderState(folder, 2) == LearningFolderState::kUnavailable) {
    return false;
  }
  std::error_code ec;
  fs::create_directories(folder, ec);
  if (ec) {
    return false;
  }
  if (!writeFileAtomically(ownFilePath(), content)) {
    return false;
  }
  localStamp_ = stampOf(ownFilePath());
  unsavedChanges_ = false;
  return true;
}

std::vector<CommonSentenceStore::SentenceEntry>
CommonSentenceStore::allEntries() {
  reload();
  const double now = wallNow_();
  std::vector<SentenceEntry> result;
  for (const auto& [sentence, entry] : mergedEntries_) {
    if (isAlive(sentence, entry, now)) {
      result.push_back({sentence, entry});
    }
  }
  std::sort(result.begin(), result.end(),
            [](const SentenceEntry& a, const SentenceEntry& b) {
              if (a.entry.count != b.entry.count) {
                return a.entry.count > b.entry.count;
              }
              if (a.entry.lastUsed != b.entry.lastUsed) {
                return a.entry.lastUsed > b.entry.lastUsed;
              }
              return a.sentence < b.sentence;
            });
  return result;
}

bool CommonSentenceStore::add(const std::string& sentence) {
  if (!IsValidSentence(sentence) || !ensureLocalFileLoaded()) {
    return false;
  }
  reload();
  if (!removedEntries_->remove(RemovedEntries::kSentence, sentence)) {
    return false;
  }
  const double now = wallNow_();
  auto it = localEntries_.find(sentence);
  Entry entry = it == localEntries_.end() ? Entry{0, now, true} : it->second;
  // A sentence dropped while removed is re-added from kThreshold, not from
  // its old count (the sentence version of report D-b).
  entry.count = std::max(entry.count, kThreshold);
  entry.lastUsed = now;
  entry.pinned = true;
  localEntries_[sentence] = entry;
  unsavedChanges_ = true;
  bool saved = save();
  reload();
  return saved;
}

bool CommonSentenceStore::remove(const std::string& sentence) {
  return removeAll({sentence});
}

bool CommonSentenceStore::removeAll(const std::vector<std::string>& sentences) {
  if (sentences.empty()) {
    return true;
  }
  if (!ensureLocalFileLoaded()) {
    return false;
  }
  reload();
  // Recorded first, so that the sentences are not brought back from the own
  // file if another program wrote it meanwhile.
  if (!removedEntries_->addAll(RemovedEntries::kSentence, sentences,
                               wallNow_())) {
    return false;
  }
  for (const auto& sentence : sentences) {
    localEntries_.erase(sentence);
  }
  unsavedChanges_ = true;
  bool saved = save();
  reload();
  return saved;
}

std::optional<CommonSentenceStore::Entry> CommonSentenceStore::entry(
    const std::string& sentence) const {
  auto it = mergedEntries_.find(sentence);
  if (it == mergedEntries_.end()) {
    return std::nullopt;
  }
  return it->second;
}

bool CommonSentenceStore::loadLocalFileIfNeeded() {
  if (!localFilePending_ || !hasFolder_) {
    return false;
  }
  const fs::path folder = sentencesFolder();
  // learning/sentences/ and learning/ may both be missing.
  switch (learningFolderState(folder, 2)) {
    case LearningFolderState::kUnavailable:
      // The user data folder may not have been synced yet.
      return false;
    case LearningFolderState::kCreatable:
      // No sentences folder yet, so no own file: start empty. Saving creates
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
        // Starting a new file would split this device's sentences in two.
        return false;
      }
    }
  }

  // Taken before reading, so that a change made meanwhile is read again.
  const FileStamp stamp = stampOf(path);
  if (fs::exists(fs::symlink_status(path, ec))) {
    auto content = readWholeFile(path);
    if (!content) {
      // Never replace a file that could not be read.
      return false;
    }
    // What was recorded meanwhile is added to the file's sentences.
    for (const auto& [sentence, entry] : parseEntries(*content)) {
      auto it = localEntries_.find(sentence);
      if (it == localEntries_.end()) {
        localEntries_[sentence] = entry;
      } else {
        combine(&it->second, entry);
      }
    }
  }
  localStamp_ = stamp;
  localFilePending_ = false;
  return true;
}

bool CommonSentenceStore::ensureLocalFileLoaded() {
  if (!hasFolder_) {
    return false;
  }
  return !localFilePending_ || loadLocalFileIfNeeded();
}

FileChange CommonSentenceStore::mergeLocalFileIfChanged() {
  if (!hasFolder_ || localFilePending_) {
    return FileChange::kUnchanged;
  }
  std::string content;
  FileChange change = readIfChanged(ownFilePath(), &localStamp_, &content);
  if (change == FileChange::kChanged) {
    // The same sentence written by both: the larger count, the later time,
    // and added by the user if either says so.
    for (const auto& [sentence, entry] : parseEntries(content)) {
      auto it = localEntries_.find(sentence);
      if (it == localEntries_.end()) {
        localEntries_[sentence] = entry;
        continue;
      }
      Entry& local = it->second;
      local.count = std::max(local.count, entry.count);
      local.lastUsed = std::max(local.lastUsed, entry.lastUsed);
      local.pinned = local.pinned || entry.pinned;
    }
  }
  return change;
}

void CommonSentenceStore::dropRemovedLocalSentences() {
  if (!hasFolder_) {
    return;
  }
  // Removed at or after the time it was last used, maybe by another program.
  for (auto it = localEntries_.begin(); it != localEntries_.end();) {
    // A time of 0 means the entry is not removed.
    double removed =
        removedEntries_->removedTime(RemovedEntries::kSentence, it->first);
    if (removed != 0 && removed >= it->second.lastUsed) {
      it = localEntries_.erase(it);
    } else {
      ++it;
    }
  }
}

void CommonSentenceStore::merge() {
  std::map<std::string, Entry> merged = localEntries_;
  // Taken before reading, so that a file changed meanwhile is read again.
  snapshot_ = takeSnapshot();
  if (hasFolder_) {
    for (const auto& path :
         otherDevicesLearningFiles(sentencesFolder(), ownFilename_)) {
      auto content = readWholeFile(path);
      if (!content) {
        continue;
      }
      for (const auto& [sentence, entry] : parseEntries(*content)) {
        combine(&merged[sentence], entry);
      }
    }
  }
  mergedEntries_ = std::move(merged);
  lastLoaded_ = wallNow_();
}

bool CommonSentenceStore::isAlive(const std::string& sentence,
                                  const Entry& entry, double now) {
  if (removedEntries_->removedTime(RemovedEntries::kSentence, sentence) >=
      entry.lastUsed) {
    return false;
  }
  return entry.pinned || now - entry.lastUsed <= lifetime(entry);
}

fs::path CommonSentenceStore::sentencesFolder() const {
  return learningFolder_ / kSentencesFolderName;
}

fs::path CommonSentenceStore::ownFilePath() const {
  return sentencesFolder() / ownFilename_;
}

}  // namespace McBopomofo::SmartLearning
