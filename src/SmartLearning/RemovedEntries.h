#ifndef SRC_SMARTLEARNING_REMOVEDENTRIES_H_
#define SRC_SMARTLEARNING_REMOVEDENTRIES_H_

#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>

#include "AtomicFile.h"

namespace McBopomofo::SmartLearning {

// The entries the user removed from the learned data, kept in "removed.txt"
// of the shared learning folder and shared by all the devices, so that a
// device does not bring back what another one removed. Each line is the
// kind, a tab, the key, a tab, and the time (Unix seconds) of the removal.
//
// Lookups read the file again at most once every kReloadInterval seconds.
// add() and remove() read it again first, then write it whole, atomically,
// sorted by line and with the times in whole seconds.
class RemovedEntries {
 public:
  static constexpr char kEnglish[] = "english";
  static constexpr char kSentence[] = "sentence";
  // The key of a temporary phrase is "<value> <reading>".
  static constexpr char kTemporary[] = "temporary";
  static constexpr double kReloadInterval = 60;  // seconds

  // |now| gives the time in seconds, used only to tell when to read the file
  // again.
  explicit RemovedEntries(std::function<double()> now);

  // Uses "<learningFolder>/removed.txt". An empty or relative path is
  // ignored. A different folder is read on the next lookup.
  void setLearningFolder(std::filesystem::path learningFolder);

  // The time the entry was removed, or 0 if it has not been removed.
  double removedTime(std::string_view kind, std::string_view key);

  // Reads the file again now, for example after it was seen to change.
  void reload();

  // Reads the file again if it changed (whether it exists, its modification
  // time or its size) since it was last read or written here. Returns true if
  // it was read again.
  bool reloadIfChanged();

  // Records the removal of the entry at |wallNow| (Unix seconds), replacing
  // an earlier one. Returns false, writing nothing, when there is no folder,
  // the key or kind holds a tab or any line break (LF, CR, VT, FF, NEL, LINE
  // SEPARATOR or PARAGRAPH SEPARATOR), the file exists but cannot
  // be read, the user data folder holding the learning folder is missing, or
  // the write fails.
  bool add(std::string_view kind, std::string_view key, double wallNow);

  // Takes the entry out. Returns true if it was taken out, or was not there
  // (nothing is written then); false, writing nothing, for a key or kind that
  // add() refuses, if the file exists but cannot be read, or if the write
  // fails.
  bool remove(std::string_view kind, std::string_view key);

 private:
  void loadIfNeeded();
  // Reads the entries of the file into |entries|; false if the file exists
  // but cannot be read.
  bool readFile(std::unordered_map<std::string, double>* entries,
                FileStamp* stamp) const;
  bool writeFile(std::unordered_map<std::string, double> entries);

  std::function<double()> now_;
  std::filesystem::path learningFolder_;
  bool loaded_ = false;
  double lastLoaded_ = 0;
  // "<kind>\t<key>" to the time.
  std::unordered_map<std::string, double> entries_;
  // The file as it was last read or written.
  FileStamp stamp_;
};

}  // namespace McBopomofo::SmartLearning

#endif  // SRC_SMARTLEARNING_REMOVEDENTRIES_H_
