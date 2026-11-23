#ifndef SRC_SMARTLEARNING_LEARNINGPATHS_H_
#define SRC_SMARTLEARNING_LEARNINGPATHS_H_

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace McBopomofo::SmartLearning {

// Returns a new device ID: 8 uppercase hexadecimal characters.
std::string newDeviceID();

// Replaces characters that are unsafe in a file name ('/', ':', '\\', '\n',
// '\r', '\t') with '_'. An empty result becomes "Linux".
std::string sanitizeDeviceName(std::string_view name);

// "<deviceID>-<deviceName>.txt"
std::string learningFilename(std::string_view deviceID,
                             std::string_view deviceName);

// True if the file name looks like a copy made by a file-sync conflict
// (contains "_Conflict" or ".sync-conflict-").
bool isSyncConflictCopy(std::string_view filename);

// Regular ".txt" files directly inside |folder|, excluding the device's own
// file, hidden files and sync conflict copies, sorted by file name. Returns an
// empty list if the folder cannot be read.
std::vector<std::filesystem::path> otherDevicesLearningFiles(
    const std::filesystem::path& folder, std::string_view ownFilename);

// Finds a ".txt" file in |folder| that carries this device's "<deviceID>-"
// prefix but a different name than |ownFilename| (for example after the
// computer was renamed). Conflict copies are never returned. When several
// match, the first in sorted file name order is returned.
std::optional<std::filesystem::path> findRenamedOwnFile(
    const std::filesystem::path& folder, std::string_view deviceID,
    std::string_view ownFilename);

// Whether a store's own learning folder can be used.
enum class LearningFolderState {
  // The folder exists.
  kExists,
  // The folder is missing, but it can be created when saving: at most
  // |missingLevels| folders, |folder| and its parents, are missing, and the
  // nearest existing parent is a folder.
  kCreatable,
  // Anything else: the user data folder itself is missing (for example a sync
  // folder that has not arrived or is not mounted), something other than a
  // folder is in the way, or the state cannot be read.
  kUnavailable,
};

LearningFolderState learningFolderState(const std::filesystem::path& folder,
                                        int missingLevels);

}  // namespace McBopomofo::SmartLearning

#endif  // SRC_SMARTLEARNING_LEARNINGPATHS_H_
