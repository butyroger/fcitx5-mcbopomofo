#include "TemporaryPhrases.h"

#include <algorithm>
#include <charconv>
#include <cstdio>
#include <string_view>
#include <system_error>
#include <utility>

#include "AtomicFile.h"
#include "LearningFileUtils.h"
#include "LearningPaths.h"

namespace McBopomofo::SmartLearning {
namespace fs = std::filesystem;

namespace {

constexpr char kTemporaryFolderName[] = "temporary";
constexpr char kRemovedFilename[] = "removed.txt";
// The snapshot key of removed.txt; no file in the folder can have it.
constexpr char kRemovedSnapshotKey[] = "/removed.txt";

// The number at the start of the text, like "12" for "12abc", or 0 if there
// is none.
double leadingNumber(std::string_view s) {
  if (!s.empty() && s.front() == '+') {
    s.remove_prefix(1);
    if (!s.empty() && s.front() == '-') {
      return 0;
    }
  }
  double value = 0;
  auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), value);
  return ec == std::errc() ? value : 0;
}

// Each line is the value, the reading and the time, separated by single
// spaces. Phrases older than the lifetime are dropped.
std::map<std::string, double> parsePhrases(std::string_view content,
                                           double now) {
  std::map<std::string, double> result;
  while (!content.empty()) {
    size_t end = content.find('\n');
    std::string_view line = content.substr(0, end);
    content = end == std::string_view::npos ? std::string_view()
                                            : content.substr(end + 1);
    size_t first = line.find(' ');
    if (first == std::string_view::npos) {
      continue;
    }
    size_t second = line.find(' ', first + 1);
    if (second == std::string_view::npos ||
        line.find(' ', second + 1) != std::string_view::npos) {
      continue;
    }
    double time = leadingNumber(line.substr(second + 1));
    if (now - time > TemporaryPhraseStore::kLifetime) {
      continue;
    }
    result[std::string(line.substr(0, second))] = time;
  }
  return result;
}

std::string phraseKey(const std::string& value, const std::string& reading) {
  return value + " " + reading;
}

}  // namespace

TemporaryPhraseStore::TemporaryPhraseStore(
    std::string deviceID, std::string deviceName,
    std::function<double()> wallNow,
    std::shared_ptr<RemovedEntries> removedEntries)
    : deviceID_(std::move(deviceID)),
      ownFilename_(learningFilename(deviceID_, sanitizeDeviceName(deviceName))),
      wallNow_(std::move(wallNow)),
      removedEntries_(removedEntries
                          ? std::move(removedEntries)
                          : std::make_shared<RemovedEntries>(steadySeconds)) {}

void TemporaryPhraseStore::setLearningFolder(fs::path learningFolder) {
  // A relative path would resolve against the process's working directory.
  if (learningFolder.empty() || !learningFolder.is_absolute()) {
    return;
  }
  if (hasFolder_ &&
      normalizedFolder(learningFolder) == normalizedFolder(learningFolder_)) {
    reload();
    return;
  }
  if (hasFolder_) {
    // The own phrases belong to the old folder: keep them there, then start
    // over with what the new folder holds.
    if (!localFilePending_ && unsavedChanges_) {
      save();
    }
    // While the old folder's own file is pending, what was added or removed
    // meanwhile is dropped: that file was never read, so writing it is unsafe.
    localPhrases_.clear();
    removedWhilePending_.clear();
    unsavedChanges_ = false;
  }
  learningFolder_ = std::move(learningFolder);
  hasFolder_ = true;
  localFilePending_ = true;
  snapshot_.clear();
  removedEntries_->setLearningFolder(learningFolder_);
  reload();
}

void TemporaryPhraseStore::reload() {
  loadLocalFileIfNeeded();
  if (hasFolder_) {
    // Read the removed entries now rather than after their reload interval.
    removedEntries_->reloadIfChanged();
  }
  mergeLocalFileIfChanged();
  merge();
}

bool TemporaryPhraseStore::reloadIfChanged() {
  if (!hasFolder_) {
    return false;
  }
  if (takeSnapshot() != snapshot_) {
    reload();
    return true;
  }
  // Nothing in the folder changed, but an own file that failed to load may
  // be loadable now.
  if (localFilePending_ && loadLocalFileIfNeeded()) {
    merge();
    return true;
  }
  return false;
}

bool TemporaryPhraseStore::contains(const std::string& value,
                                    const std::string& reading) const {
  return phrases_.count(phraseKey(value, reading)) > 0;
}

void TemporaryPhraseStore::add(const std::string& value,
                               const std::string& reading) {
  // A space or a line break would break the line format of the file.
  if (!isPlainPart(value) || !isPlainPart(reading)) {
    return;
  }
  const std::string key = phraseKey(value, reading);
  const double now = wallNow_();
  localPhrases_[key] = now;
  phrases_[key] = now;
  removedWhilePending_.erase(key);
  unsavedChanges_ = true;
}

void TemporaryPhraseStore::remove(const std::string& value,
                                  const std::string& reading) {
  const std::string key = phraseKey(value, reading);
  if (localFilePending_) {
    // It may be in the own file, which has not been read yet.
    removedWhilePending_.insert(key);
  }
  if (phrases_.erase(key) == 0) {
    return;
  }
  // A phrase only another device has leaves the own file as it is.
  if (localPhrases_.erase(key) > 0) {
    unsavedChanges_ = true;
  }
}

std::unordered_map<std::string, std::vector<std::string>>
TemporaryPhraseStore::phrasesByReading() const {
  std::unordered_map<std::string, std::vector<std::pair<double, std::string>>>
      timed;
  for (const auto& [key, time] : phrases_) {
    size_t space = key.find(' ');
    timed[key.substr(space + 1)].emplace_back(time, key.substr(0, space));
  }
  std::unordered_map<std::string, std::vector<std::string>> result;
  for (auto& [reading, values] : timed) {
    std::sort(values.begin(), values.end(), [](const auto& a, const auto& b) {
      return a.first != b.first ? a.first > b.first : a.second < b.second;
    });
    auto& out = result[reading];
    for (auto& [time, value] : values) {
      out.push_back(std::move(value));
    }
  }
  return result;
}

bool TemporaryPhraseStore::save() {
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
      merge();
      break;
    case FileChange::kUnchanged:
      break;
  }

  std::string content;
  for (const auto& [key, time] : localPhrases_) {
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%.0f", time);
    content += key;
    content += ' ';
    content += buffer;
    content += '\n';
  }

  // Create learning/temporary/ when it is missing, but never the user data
  // folder itself: a missing one may be a sync folder that is not mounted.
  const fs::path folder = temporaryFolder();
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
  unsavedChanges_ = false;
  localStamp_ = stampOf(ownFilePath());
  recordOwnFileInSnapshot();
  return true;
}

std::vector<TemporaryPhraseStore::TimedPhrase>
TemporaryPhraseStore::allPhrases() {
  reload();
  std::vector<TimedPhrase> result;
  result.reserve(phrases_.size());
  for (const auto& [key, time] : phrases_) {
    size_t space = key.find(' ');
    result.push_back({key.substr(0, space), key.substr(space + 1), time});
  }
  std::sort(result.begin(), result.end(),
            [](const TimedPhrase& a, const TimedPhrase& b) {
              if (a.time != b.time) {
                return a.time > b.time;
              }
              return a.value != b.value ? a.value < b.value
                                        : a.reading < b.reading;
            });
  return result;
}

bool TemporaryPhraseStore::forget(const std::string& value,
                                  const std::string& reading) {
  return forgetAll({{value, reading}});
}

bool TemporaryPhraseStore::forgetAll(
    const std::vector<std::pair<std::string, std::string>>& phrases) {
  if (phrases.empty()) {
    return true;
  }
  if (!ensureLocalFileLoaded()) {
    return false;
  }
  std::vector<std::string> keys;
  keys.reserve(phrases.size());
  for (const auto& [value, reading] : phrases) {
    keys.push_back(phraseKey(value, reading));
  }
  // Recorded first, so that the phrases are not brought back from the own
  // file if another program wrote it meanwhile.
  if (!removedEntries_->addAll(RemovedEntries::kTemporary, keys,
                               wallNow_())) {
    return false;
  }
  for (const auto& key : keys) {
    localPhrases_.erase(key);
    phrases_.erase(key);
  }
  unsavedChanges_ = true;
  return save();
}

bool TemporaryPhraseStore::loadLocalFileIfNeeded() {
  if (!localFilePending_ || !hasFolder_) {
    return false;
  }
  const fs::path folder = temporaryFolder();
  // learning/temporary/ and learning/ may both be missing.
  switch (learningFolderState(folder, 2)) {
    case LearningFolderState::kUnavailable:
      // The user data folder may not have been synced yet.
      return false;
    case LearningFolderState::kCreatable:
      // No temporary folder yet, so no own file: start empty. Saving creates
      // the folder.
      removedWhilePending_.clear();
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
        // Starting a new file would split this device's phrases in two.
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
    // What was added meanwhile is kept, the newer time winning.
    for (const auto& [key, time] : parsePhrases(*content, wallNow_())) {
      if (removedWhilePending_.count(key) > 0) {
        continue;
      }
      auto it = localPhrases_.find(key);
      if (it == localPhrases_.end() || it->second < time) {
        localPhrases_[key] = time;
      }
    }
  }
  removedWhilePending_.clear();
  localStamp_ = stamp;
  localFilePending_ = false;
  return true;
}

bool TemporaryPhraseStore::ensureLocalFileLoaded() {
  if (!hasFolder_) {
    return false;
  }
  return !localFilePending_ || loadLocalFileIfNeeded();
}

FileChange TemporaryPhraseStore::mergeLocalFileIfChanged() {
  if (!hasFolder_ || localFilePending_) {
    return FileChange::kUnchanged;
  }
  std::string content;
  FileChange change = readIfChanged(ownFilePath(), &localStamp_, &content);
  if (change == FileChange::kChanged) {
    // The later time wins.
    for (const auto& [key, time] : parsePhrases(content, wallNow_())) {
      auto it = localPhrases_.find(key);
      if (it == localPhrases_.end() || it->second < time) {
        localPhrases_[key] = time;
      }
    }
  }
  return change;
}

void TemporaryPhraseStore::merge() {
  // Removed on any device after they were picked.
  for (auto it = localPhrases_.begin(); it != localPhrases_.end();) {
    if (isRemoved(it->first, it->second)) {
      it = localPhrases_.erase(it);
    } else {
      ++it;
    }
  }
  std::map<std::string, double> merged = localPhrases_;
  if (hasFolder_) {
    const double now = wallNow_();
    for (const auto& path :
         otherDevicesLearningFiles(temporaryFolder(), ownFilename_)) {
      auto content = readWholeFile(path);
      if (!content) {
        continue;
      }
      for (const auto& [key, time] : parsePhrases(*content, now)) {
        if (isRemoved(key, time)) {
          continue;
        }
        auto it = merged.find(key);
        if (it == merged.end() || it->second < time) {
          merged[key] = time;
        }
      }
    }
    snapshot_ = takeSnapshot();
  }
  phrases_ = std::move(merged);
}

bool TemporaryPhraseStore::isRemoved(const std::string& phrase, double time) {
  // The own file rounds the pick time (%.0f) while removed.txt truncates the
  // removal time, so a phrase picked and forgotten within the same second on
  // different programs may stay listed (report D-e).
  return removedEntries_->removedTime(RemovedEntries::kTemporary, phrase) >=
         time;
}

TemporaryPhraseStore::Snapshot TemporaryPhraseStore::takeSnapshot() const {
  Snapshot snapshot = txtFileStamps(temporaryFolder());
  FileStamp removed = stampOf(learningFolder_ / kRemovedFilename);
  if (removed.exists) {
    snapshot[kRemovedSnapshotKey] = removed;
  }
  return snapshot;
}

void TemporaryPhraseStore::recordOwnFileInSnapshot() {
  // The own write is not a change made by another device.
  FileStamp stamp = stampOf(ownFilePath());
  if (stamp.exists) {
    snapshot_[ownFilename_] = stamp;
  }
}

fs::path TemporaryPhraseStore::temporaryFolder() const {
  return learningFolder_ / kTemporaryFolderName;
}

fs::path TemporaryPhraseStore::ownFilePath() const {
  return temporaryFolder() / ownFilename_;
}

}  // namespace McBopomofo::SmartLearning
