#ifndef SRC_SMARTLEARNING_TEMPORARYPHRASES_H_
#define SRC_SMARTLEARNING_TEMPORARYPHRASES_H_

#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "AtomicFile.h"
#include "RemovedEntries.h"

namespace McBopomofo::SmartLearning {

// Phrases picked once, kept for a while before they are learned for good.
// They come right after the user phrases and before the built-in ones.
//
// Each device writes only its own "<deviceID>-<deviceName>.txt" in the
// "temporary" folder of the shared learning folder; each line is the value,
// the reading and the time it was picked (Unix seconds), separated by single
// spaces. On reading, the phrases of all the devices are merged, the newer
// time winning. A phrase picked more than kLifetime seconds ago is dropped
// when its file is read, and one removed (an entry of kind "temporary" in
// "removed.txt") at or after the time it was picked is left out.
//
// The own file is never written before it has been read: until then (the
// user data folder does not exist yet, or the file cannot be read) the store
// is pending, and what is added or removed meanwhile is applied to the file's
// phrases once it loads. A missing "temporary" folder in an existing user
// data folder is an empty own file; saving creates it. Another program (the
// vocabulary manager) may write the own file too: when its stamp changed, it
// is read again before reloading and before saving, the later time of each
// phrase winning. A phrase missing from the file is not taken as removed;
// only "removed.txt" removes.
class TemporaryPhraseStore {
 public:
  static constexpr double kLifetime = 30 * 86400;  // seconds

  struct TimedPhrase {
    std::string value;
    std::string reading;
    // When it was picked, in Unix seconds.
    double time = 0;
  };

  // |wallNow| gives the current Unix time in seconds: the times are compared
  // across devices. Without |removedEntries|, the store reads "removed.txt"
  // with its own reader.
  TemporaryPhraseStore(
      std::string deviceID, std::string deviceName,
      std::function<double()> wallNow,
      std::shared_ptr<RemovedEntries> removedEntries = nullptr);

  // Uses "<learningFolder>/temporary" and "<learningFolder>/removed.txt". An
  // empty or relative path is ignored. Setting the current folder again only
  // reloads. When switching folders, the own phrases are saved to the old
  // folder (if its file was loaded), then replaced by what the new folder's
  // own file holds.
  void setLearningFolder(std::filesystem::path learningFolder);

  // Loads the own file if still pending, reads it again if another program
  // changed it, then merges the other devices' files again.
  void reload();

  // Reloads if the ".txt" files of the temporary folder or "removed.txt"
  // (name, modification time, size) changed since the last load or save,
  // and retries loading the own file while it is pending. Returns true if
  // the phrases were merged again.
  bool reloadIfChanged();

  bool contains(const std::string& value, const std::string& reading) const;

  // Adds the phrase, picked now. A value or reading that is empty or holds a
  // space, a tab or a line break is ignored. The file is written by save().
  void add(const std::string& value, const std::string& reading);

  // Removes the phrase if it is there, as when it is learned for good. The
  // file is written by save(); a phrase that is only in other devices' files
  // is removed from memory only, leaving no unsaved change. Nothing is
  // written to "removed.txt".
  void remove(const std::string& value, const std::string& reading);

  // For the vocabulary manager: the phrases of all the devices, read again
  // now, the most recently picked first.
  std::vector<TimedPhrase> allPhrases();

  // Forgets a phrase: records it in "removed.txt", which hides it on all the
  // devices until it is picked again, takes it out of the own phrases and
  // saves. Returns false, changing nothing, while the own file is pending,
  // or if "removed.txt" cannot be read or written; false also if the own
  // file cannot be written.
  bool forget(const std::string& value, const std::string& reading);

  // The values of each reading, the most recently picked first.
  std::unordered_map<std::string, std::vector<std::string>> phrasesByReading()
      const;

  // Writes the own phrases to the own file atomically. Returns false,
  // writing nothing, while the own file is pending or if the write fails.
  bool save();

  [[nodiscard]] bool hasUnsavedChanges() const { return unsavedChanges_; }
  [[nodiscard]] bool isLocalFilePending() const { return localFilePending_; }

 private:
  // The files of the temporary folder and removed.txt, by name.
  using Snapshot = std::map<std::string, FileStamp>;

  // Returns true if the own file was loaded (or found absent) by this call.
  bool loadLocalFileIfNeeded();
  // Returns false if the own file could not be loaded.
  bool ensureLocalFileLoaded();
  // Merges the own file if another program changed it.
  FileChange mergeLocalFileIfChanged();
  void merge();
  bool isRemoved(const std::string& phrase, double time);
  Snapshot takeSnapshot() const;
  void recordOwnFileInSnapshot();
  std::filesystem::path temporaryFolder() const;
  std::filesystem::path ownFilePath() const;

  std::string deviceID_;
  std::string ownFilename_;
  std::function<double()> wallNow_;
  std::shared_ptr<RemovedEntries> removedEntries_;

  std::filesystem::path learningFolder_;
  bool hasFolder_ = false;
  bool localFilePending_ = true;
  bool unsavedChanges_ = false;
  Snapshot snapshot_;
  // The own file as it was last read or written here.
  FileStamp localStamp_;

  // "<value> <reading>" to the time it was picked, of this device and of all
  // the devices.
  std::map<std::string, double> localPhrases_;
  std::map<std::string, double> phrases_;
  // Removed while the own file was pending, to be left out when it loads.
  std::set<std::string> removedWhilePending_;
};

}  // namespace McBopomofo::SmartLearning

#endif  // SRC_SMARTLEARNING_TEMPORARYPHRASES_H_
