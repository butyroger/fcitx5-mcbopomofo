#ifndef SRC_SMARTLEARNING_REMOVEDENTRIES_H_
#define SRC_SMARTLEARNING_REMOVEDENTRIES_H_

#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace McBopomofo::SmartLearning {

// The entries the user removed from the learned data, kept in "removed.txt"
// of the shared learning folder and shared by all the devices, so that a
// device does not bring back what another one removed. Each line is the
// kind, a tab, the key, a tab, and the time (Unix seconds) of the removal.
//
// This class only reads the file; it is read again at most once every
// kReloadInterval seconds.
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

 private:
  void loadIfNeeded();

  std::function<double()> now_;
  std::filesystem::path learningFolder_;
  bool loaded_ = false;
  double lastLoaded_ = 0;
  // "<kind>\t<key>" to the time.
  std::unordered_map<std::string, double> entries_;
};

}  // namespace McBopomofo::SmartLearning

#endif  // SRC_SMARTLEARNING_REMOVEDENTRIES_H_
