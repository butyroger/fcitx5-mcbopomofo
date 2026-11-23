#ifndef SRC_SMARTLEARNING_USEROVERRIDESTORE_H_
#define SRC_SMARTLEARNING_USEROVERRIDESTORE_H_

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <utility>

#include "UserOverrideModel.h"
#include "gramambular2/reading_grid.h"

namespace McBopomofo::SmartLearning {

// Keeps the candidates the user picked, per device, in a shared learning
// folder. Each device writes only its own "<deviceID>-<deviceName>.txt"; the
// suggestions come from a merged model that also holds what the other devices
// in the folder have saved.
//
// The own file is never written before it has been loaded, so that a file
// still being synced from another computer is not replaced by a nearly empty
// one. Until it loads, the store is "pending": observations still go into the
// models and are kept when the file eventually loads.
class UserOverrideStore {
 public:
  static constexpr size_t kLocalCapacity = 5000;
  static constexpr size_t kMergedCapacity = 20000;
  static constexpr double kHalfLife = 30.0 * 86400.0;  // 30 days.

  UserOverrideStore(std::string deviceID, std::string deviceName);

  // Uses the learning folder. An empty or relative path is ignored. Setting the current folder again only reloads.
  // When switching folders, the own model is saved to the old folder (if its
  // file was loaded), then replaced by what the new folder's own file holds.
  void setLearningFolder(std::filesystem::path folder);

  // The model the suggestions come from. The pointer stays valid for the
  // lifetime of the store.
  UserOverrideModel* mergedModel();

  // Records the choice in both the own model and the merged model.
  void observe(const Formosa::Gramambular2::ReadingGrid::WalkResult& before,
               const Formosa::Gramambular2::ReadingGrid::WalkResult& after,
               size_t cursor, double timestamp);

  // Reloads if the ".txt" files in the folder (name, modification time,
  // size) or the existence of the folder changed since the last load or
  // save, and retries loading the own file while it is pending. Returns true
  // if the merged model was rebuilt.
  bool reloadIfChanged();

  // Loads the own file if still pending, then rebuilds the merged model from
  // the own model plus the other devices' files in file name order.
  void reload();

  // Writes the own model to the own file atomically. Returns false, writing
  // nothing, while the own file is pending or if the write fails.
  bool save();

  [[nodiscard]] bool isLocalFilePending() const;

 private:
  struct FileStamp {
    std::filesystem::file_time_type mtime;
    std::uintmax_t size = 0;
    bool operator==(const FileStamp& other) const {
      return mtime == other.mtime && size == other.size;
    }
  };
  struct Snapshot {
    bool folderExists = false;
    std::map<std::string, FileStamp> files;
    bool operator==(const Snapshot& other) const {
      return folderExists == other.folderExists && files == other.files;
    }
  };

  // Returns true if the own file was loaded (or found absent) by this call.
  bool loadLocalFileIfNeeded();
  void rebuildMergedModel();
  Snapshot takeSnapshot() const;
  std::filesystem::path ownFilePath() const;

  std::string deviceID_;
  std::string ownFilename_;
  std::filesystem::path folder_;
  bool hasFolder_ = false;
  bool localFilePending_ = true;
  Snapshot snapshot_;
  UserOverrideModel localModel_;
  UserOverrideModel mergedModel_;
};

}  // namespace McBopomofo::SmartLearning

#endif  // SRC_SMARTLEARNING_USEROVERRIDESTORE_H_
