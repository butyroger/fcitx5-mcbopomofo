#ifndef SRC_SMARTLEARNING_COMMONSENTENCES_H_
#define SRC_SMARTLEARNING_COMMONSENTENCES_H_

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "RemovedEntries.h"

namespace McBopomofo::SmartLearning {

// The length of a sentence: each character counts one, a run of ASCII letters
// or digits (an English word or a number) counts one, and spaces do not
// count.
size_t SentenceLength(std::string_view sentence);

// Whether a sentence can be remembered: 6 to 40 by SentenceLength, without
// any separator (punctuation, symbols such as emoji, line breaks or tabs).
bool IsValidSentence(std::string_view sentence);

// The sentences in committed text: the text is split at the separators, each
// part is trimmed of spaces at both ends, and the valid ones are kept.
// Spaces do not split, so "hello 你好" stays one part.
std::vector<std::string> SplitSentences(std::string_view committed);

// Sentences the user types often. A sentence committed kThreshold times is
// remembered, and then the rest of it is suggested after its beginning is
// typed, to be taken with Tab.
//
// Each device writes only its own "<deviceID>-<deviceName>.txt" in the
// "sentences" folder of the shared learning folder; each line is the
// sentence, the count, the time last used (Unix seconds) and 1 if the user
// added it (0 otherwise), separated by tabs. On reading, the sentences of all
// the devices are merged: the counts are added, the later time wins, and a
// sentence added by the user on any device stays added. A sentence removed
// (an entry of kind "sentence" in "removed.txt") at or after the time it was
// last used is never suggested. A sentence not used for its lifetime (see
// lifetime()) is not suggested, and is dropped from the own file on saving,
// unless the user added it.
//
// The own file is read only once, and is never written before it has been
// read: until then (the user data folder does not exist yet, or the file
// cannot be read) the store is pending, and what is recorded meanwhile is
// added to the file's sentences once it loads. A missing "sentences" folder
// in an existing user data folder is an empty own file; saving creates it.
//
// Recording and looking up only use memory; the files are read by
// setLearningFolder(), reload() and reloadIfNeeded(), and written by save().
class CommonSentenceStore {
 public:
  static constexpr long long kThreshold = 3;
  static constexpr size_t kMinimumLength = 6;
  static constexpr size_t kMaximumLength = 40;
  static constexpr size_t kMinimumPrefixLength = 2;
  static constexpr double kReloadInterval = 60;  // seconds

  struct Entry {
    long long count = 0;
    double lastUsed = 0;
    // Added by the user, which is never forgotten.
    bool pinned = false;
  };

  // How long a sentence is kept without being used: 90 days, and 30 more for
  // every three times it has been used beyond kThreshold.
  static double lifetime(const Entry& entry);

  // |wallNow| gives the current Unix time in seconds: the times are compared
  // across devices. Without |removedEntries|, the store reads "removed.txt"
  // with its own reader.
  CommonSentenceStore(std::string deviceID, std::string deviceName,
                      std::function<double()> wallNow,
                      std::shared_ptr<RemovedEntries> removedEntries = nullptr);

  // Uses "<learningFolder>/sentences" and "<learningFolder>/removed.txt". An
  // empty or relative path is ignored. Setting the current folder again only
  // reloads. When switching folders, the own sentences are saved to the old
  // folder (if its file was loaded), then replaced by what the new folder's
  // own file holds.
  void setLearningFolder(std::filesystem::path learningFolder);

  // When turned off, nothing is recorded or suggested.
  void setEnabled(bool enabled) { enabled_ = enabled; }
  [[nodiscard]] bool isEnabled() const { return enabled_; }

  // Counts the sentences in the committed text (see SplitSentences), used
  // now. The file is written by save().
  void record(std::string_view committed);

  // The rest of the remembered sentence that starts with what has been typed
  // since the beginning of |typedHead| or its last separator (leading spaces
  // left out), which must be at least kMinimumPrefixLength characters. Of the
  // sentences used kThreshold times or added by the user, the one used most
  // wins, then the one used last.
  std::optional<std::string> completion(std::string_view typedHead);

  // Loads the own file if still pending, then merges the other devices' files
  // again.
  void reload();

  // Reloads if the files were last read kReloadInterval seconds ago or more
  // and the other devices' files (name, modification time, size) changed
  // since, or the own file is still pending.
  void reloadIfNeeded();

  // Writes the own sentences to the own file atomically, leaving out those
  // past their lifetime. Returns false, writing nothing, while the own file
  // is pending or if the write fails.
  bool save();

  [[nodiscard]] bool hasUnsavedChanges() const { return unsavedChanges_; }
  [[nodiscard]] bool isLocalFilePending() const { return localFilePending_; }

  // The merged entry of a sentence, if any; for tests.
  [[nodiscard]] std::optional<Entry> entry(const std::string& sentence) const;

 private:
  // The modification time and size of each other device's file, by name.
  using Snapshot =
      std::map<std::string,
               std::pair<std::filesystem::file_time_type, std::uintmax_t>>;

  // Returns true if the own file was loaded (or found absent) by this call.
  bool loadLocalFileIfNeeded();
  Snapshot takeSnapshot() const;
  void merge();
  bool isAlive(const std::string& sentence, const Entry& entry, double now);
  std::filesystem::path sentencesFolder() const;
  std::filesystem::path ownFilePath() const;

  std::string deviceID_;
  std::string ownFilename_;
  std::function<double()> wallNow_;
  std::shared_ptr<RemovedEntries> removedEntries_;
  bool enabled_ = true;

  std::filesystem::path learningFolder_;
  bool hasFolder_ = false;
  bool localFilePending_ = true;
  bool unsavedChanges_ = false;
  double lastLoaded_ = 0;
  // The other devices' files as they were last merged.
  Snapshot snapshot_;

  // The sentences of this device, and of all the devices.
  std::map<std::string, Entry> localEntries_;
  std::map<std::string, Entry> mergedEntries_;
};

}  // namespace McBopomofo::SmartLearning

#endif  // SRC_SMARTLEARNING_COMMONSENTENCES_H_
