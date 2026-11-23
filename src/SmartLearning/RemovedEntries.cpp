#include "RemovedEntries.h"

#include <algorithm>
#include <optional>
#include <system_error>
#include <utility>
#include <vector>

#include "AtomicFile.h"
#include "LearningFileUtils.h"
#include "LearningPaths.h"

namespace McBopomofo::SmartLearning {
namespace fs = std::filesystem;

namespace {

constexpr char kRemovedFilename[] = "removed.txt";

// A tab or any line break (as splitLines() sees them) would break the line
// format of the file.
bool breaksLineFormat(std::string_view s) {
  return s.find('\t') != std::string_view::npos || splitLines(s).size() != 1;
}

std::string entryKey(std::string_view kind, std::string_view key) {
  std::string result(kind);
  result += '\t';
  result += key;
  return result;
}

}  // namespace

RemovedEntries::RemovedEntries(std::function<double()> now)
    : now_(std::move(now)) {}

void RemovedEntries::setLearningFolder(fs::path learningFolder) {
  // A relative path would resolve against the process's working directory.
  if (learningFolder.empty() || !learningFolder.is_absolute()) {
    return;
  }
  if (normalizedFolder(learningFolder) != normalizedFolder(learningFolder_)) {
    learningFolder_ = std::move(learningFolder);
    loaded_ = false;
  }
}

double RemovedEntries::removedTime(std::string_view kind,
                                   std::string_view key) {
  loadIfNeeded();
  auto it = entries_.find(entryKey(kind, key));
  return it == entries_.end() ? 0 : it->second;
}

void RemovedEntries::loadIfNeeded() {
  if (loaded_ && now_() - lastLoaded_ < kReloadInterval) {
    return;
  }
  reload();
}

void RemovedEntries::reload() {
  std::unordered_map<std::string, double> entries;
  FileStamp stamp;
  // A file that cannot be read gives no entries.
  readFile(&entries, &stamp);
  entries_ = std::move(entries);
  stamp_ = stamp;
  loaded_ = true;
  lastLoaded_ = now_();
}

bool RemovedEntries::reloadIfChanged() {
  if (learningFolder_.empty()) {
    return false;
  }
  if (loaded_ && stampOf(learningFolder_ / kRemovedFilename) == stamp_) {
    return false;
  }
  reload();
  return true;
}

bool RemovedEntries::add(std::string_view kind, std::string_view key,
                         double wallNow) {
  return addAll(kind, {std::string(key)}, wallNow);
}

bool RemovedEntries::addAll(std::string_view kind,
                            const std::vector<std::string>& keys,
                            double wallNow) {
  if (learningFolder_.empty() || breaksLineFormat(kind)) {
    return false;
  }
  for (const auto& key : keys) {
    if (breaksLineFormat(key)) {
      return false;
    }
  }
  if (keys.empty()) {
    return true;
  }
  std::unordered_map<std::string, double> entries;
  FileStamp stamp;
  if (!readFile(&entries, &stamp)) {
    return false;
  }
  for (const auto& key : keys) {
    entries[entryKey(kind, key)] = wallNow;
  }
  return writeFile(std::move(entries));
}

bool RemovedEntries::remove(std::string_view kind, std::string_view key) {
  if (learningFolder_.empty() || breaksLineFormat(kind) ||
      breaksLineFormat(key)) {
    return false;
  }
  std::unordered_map<std::string, double> entries;
  FileStamp stamp;
  if (!readFile(&entries, &stamp)) {
    return false;
  }
  if (entries.erase(entryKey(kind, key)) == 0) {
    entries_ = std::move(entries);
    stamp_ = stamp;
    loaded_ = true;
    lastLoaded_ = now_();
    return true;
  }
  return writeFile(std::move(entries));
}

bool RemovedEntries::readFile(std::unordered_map<std::string, double>* entries,
                              FileStamp* stamp) const {
  entries->clear();
  *stamp = FileStamp();
  if (learningFolder_.empty()) {
    return true;
  }
  const fs::path path = learningFolder_ / kRemovedFilename;
  // Taken before reading, so that a change made meanwhile is read again.
  *stamp = stampOf(path);
  std::error_code ec;
  if (!fs::exists(fs::symlink_status(path, ec))) {
    return true;
  }
  auto content = readWholeFile(path);
  if (!content) {
    return false;
  }
  for (std::string_view line : split(*content, '\n', /*keepEmpty=*/false)) {
    // Exactly three columns, empty ones included.
    auto parts = split(line, '\t', /*keepEmpty=*/true);
    if (parts.size() != 3) {
      continue;
    }
    if (auto time = parseNumber(parts[2])) {
      (*entries)[entryKey(parts[0], parts[1])] = *time;
    }
  }
  return true;
}

bool RemovedEntries::writeFile(
    std::unordered_map<std::string, double> entries) {
  std::vector<std::string> lines;
  lines.reserve(entries.size());
  for (const auto& [key, time] : entries) {
    lines.push_back(key + "\t" + std::to_string(static_cast<long long>(time)) +
                    "\n");
  }
  std::sort(lines.begin(), lines.end());
  std::string content;
  for (const auto& line : lines) {
    content += line;
  }

  // Create learning/ when it is missing, but never the user data folder
  // itself: a missing one may be a sync folder that is not mounted.
  if (learningFolderState(learningFolder_, 1) ==
      LearningFolderState::kUnavailable) {
    return false;
  }
  std::error_code ec;
  fs::create_directories(learningFolder_, ec);
  if (ec) {
    return false;
  }
  const fs::path path = learningFolder_ / kRemovedFilename;
  if (!writeFileAtomically(path, content)) {
    return false;
  }
  // The exact times are kept in memory; the file holds whole seconds.
  entries_ = std::move(entries);
  stamp_ = stampOf(path);
  loaded_ = true;
  lastLoaded_ = now_();
  return true;
}

}  // namespace McBopomofo::SmartLearning
