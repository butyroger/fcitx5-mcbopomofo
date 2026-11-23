#include "UserOverrideStore.h"

#include <fstream>
#include <optional>
#include <sstream>
#include <system_error>

#include "AtomicFile.h"
#include "LearningPaths.h"

namespace McBopomofo::SmartLearning {
namespace fs = std::filesystem;

namespace {

// Reads the whole file. Returns nullopt if it is not a regular file or
// cannot be read completely.
std::optional<std::string> readWholeFile(const fs::path& path) {
  std::error_code ec;
  if (!fs::is_regular_file(path, ec)) {
    return std::nullopt;
  }
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    return std::nullopt;
  }
  std::ostringstream ss;
  ss << in.rdbuf();
  if (in.bad()) {
    return std::nullopt;
  }
  return ss.str();
}

bool hasTxtExtension(const fs::path& path) { return path.extension() == ".txt"; }

// "a/./b/" and "a/b" are the same folder.
fs::path normalizedFolder(const fs::path& path) {
  fs::path normal = path.lexically_normal();
  if (normal.filename().empty() && normal.has_parent_path()) {
    normal = normal.parent_path();
  }
  return normal;
}

}  // namespace

UserOverrideStore::UserOverrideStore(std::string deviceID,
                                     std::string deviceName)
    : deviceID_(std::move(deviceID)),
      ownFilename_(learningFilename(deviceID_, sanitizeDeviceName(deviceName))),
      localModel_(kLocalCapacity, kHalfLife),
      mergedModel_(kMergedCapacity, kHalfLife) {}

void UserOverrideStore::setLearningFolder(fs::path folder) {
  // A relative path would resolve against the process's working directory.
  if (folder.empty() || !folder.is_absolute()) {
    return;
  }
  if (hasFolder_ && normalizedFolder(folder) == normalizedFolder(folder_)) {
    // The own file is already loaded (or still pending) from this folder;
    // loading it again would add its counts a second time.
    reload();
    return;
  }
  if (hasFolder_) {
    // The own model belongs to the old folder: keep it there, then start
    // over with what the new folder holds.
    if (!localFilePending_) {
      save();
    }
    localModel_ = UserOverrideModel(kLocalCapacity, kHalfLife);
  }
  folder_ = std::move(folder);
  hasFolder_ = true;
  localFilePending_ = true;
  snapshot_ = Snapshot();
  reload();
}

UserOverrideModel* UserOverrideStore::mergedModel() { return &mergedModel_; }

void UserOverrideStore::observe(
    const Formosa::Gramambular2::ReadingGrid::WalkResult& before,
    const Formosa::Gramambular2::ReadingGrid::WalkResult& after, size_t cursor,
    double timestamp) {
  localModel_.observe(before, after, cursor, timestamp);
  mergedModel_.observe(before, after, cursor, timestamp);
}

bool UserOverrideStore::reloadIfChanged() {
  if (!hasFolder_) {
    return false;
  }
  if (takeSnapshot() != snapshot_) {
    reload();
    return true;
  }
  // Nothing in the folder changed, but an own file that failed to read may
  // be readable now.
  if (localFilePending_ && loadLocalFileIfNeeded()) {
    rebuildMergedModel();
    return true;
  }
  return false;
}

void UserOverrideStore::reload() {
  loadLocalFileIfNeeded();
  rebuildMergedModel();
}

bool UserOverrideStore::save() {
  if (!hasFolder_) {
    return false;
  }
  // Never overwrite the own file before it has been loaded.
  if (localFilePending_) {
    if (!loadLocalFileIfNeeded()) {
      return false;
    }
    rebuildMergedModel();
  }

  // Create learning/ when it is missing, but never the user data folder
  // itself: a missing one may be a sync folder that is not mounted.
  if (learningFolderState(folder_, 1) == LearningFolderState::kUnavailable) {
    return false;
  }
  std::error_code ec;
  fs::create_directories(folder_, ec);
  if (ec) {
    return false;
  }
  const fs::path path = ownFilePath();
  if (!writeFileAtomically(path, localModel_.serialize())) {
    return false;
  }

  // Record the own write so that it is not taken for a change made by
  // another device.
  snapshot_.folderExists = true;
  std::error_code mtimeEc;
  std::error_code sizeEc;
  FileStamp stamp;
  stamp.mtime = fs::last_write_time(path, mtimeEc);
  stamp.size = fs::file_size(path, sizeEc);
  if (!mtimeEc && !sizeEc) {
    snapshot_.files[ownFilename_] = stamp;
  }
  return true;
}

bool UserOverrideStore::isLocalFilePending() const {
  return localFilePending_;
}

bool UserOverrideStore::loadLocalFileIfNeeded() {
  if (!localFilePending_ || !hasFolder_) {
    return false;
  }
  switch (learningFolderState(folder_, 1)) {
    case LearningFolderState::kUnavailable:
      // The user data folder may not have been synced yet.
      return false;
    case LearningFolderState::kCreatable:
      // No learning folder yet, so no own file: start empty. Saving creates
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
    if (auto renamed = findRenamedOwnFile(folder_, deviceID_, ownFilename_)) {
      std::error_code renameEc;
      fs::rename(*renamed, path, renameEc);
      if (renameEc) {
        // Starting a new file would split this device's history in two.
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
    // Appending keeps what was observed while the file was not available.
    localModel_.deserialize(*content, /*append=*/true);
  }
  localFilePending_ = false;
  return true;
}

void UserOverrideStore::rebuildMergedModel() {
  mergedModel_.deserialize(localModel_.serialize());
  if (hasFolder_) {
    for (const auto& path : otherDevicesLearningFiles(folder_, ownFilename_)) {
      if (auto content = readWholeFile(path)) {
        mergedModel_.deserialize(*content, /*append=*/true);
      }
    }
  }
  snapshot_ = takeSnapshot();
}

UserOverrideStore::Snapshot UserOverrideStore::takeSnapshot() const {
  Snapshot snapshot;
  std::error_code ec;
  snapshot.folderExists = fs::is_directory(folder_, ec);
  if (!snapshot.folderExists) {
    return snapshot;
  }
  fs::directory_iterator it(folder_, ec);
  if (ec) {
    return snapshot;
  }
  for (const fs::directory_iterator end; it != end; it.increment(ec)) {
    if (ec) {
      break;
    }
    const auto& entry = *it;
    std::error_code entryEc;
    if (!hasTxtExtension(entry.path()) || !entry.is_regular_file(entryEc)) {
      continue;
    }
    FileStamp stamp;
    stamp.mtime = entry.last_write_time(entryEc);
    stamp.size = entry.file_size(entryEc);
    snapshot.files[entry.path().filename().string()] = stamp;
  }
  return snapshot;
}

fs::path UserOverrideStore::ownFilePath() const {
  return folder_ / ownFilename_;
}

}  // namespace McBopomofo::SmartLearning
