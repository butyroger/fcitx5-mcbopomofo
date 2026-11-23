#ifndef SRC_VOCABULARYMANAGER_LEARNEDVOCABULARY_H_
#define SRC_VOCABULARYMANAGER_LEARNEDVOCABULARY_H_

#include <cstddef>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "FileVocabulary.h"

namespace McBopomofo::SmartLearning {
class CommonSentenceStore;
class EnglishWordStore;
class RemovedEntries;
class TemporaryPhraseStore;
}  // namespace McBopomofo::SmartLearning

// The tabs of the vocabulary manager that list what has been learned: the
// English words, the common sentences and the temporary phrases. They are
// read and written through the same stores as the input method's, as the
// same device. Nothing here depends on Qt.
//
// The window holds no unsaved change, so every action reads the files again
// from scratch: what the input method took out of this device's files
// meanwhile (a temporary phrase it learned for good, for example) is not
// written back from a copy kept in memory.
namespace McBopomofo::VocabularyManager {

enum class LearnedKind { kEnglish, kSentences, kTemporary };

// The texts of the status column of the common sentences.
struct SentenceStatusTexts {
  // Added by the user.
  std::string added;
  // Sent kThreshold times or more.
  std::string remembered;
  // The times still needed to remember the sentence.
  std::function<std::string(long long)> moreTimes;
};

// Whether |word| can be added as an English word: two or more ASCII letters.
bool isAddableEnglishWord(std::string_view word);

// The date of |unixTime| in the local time zone, as "yyyy-MM-dd".
std::string localDate(double unixTime);

// The result of an action on several entries.
struct BatchResult {
  DeleteStatus status;
  // The entries done, which may be fewer than asked for when a write fails.
  size_t done = 0;
};

class LearnedVocabulary {
 public:
  // The files in "<dataFolder>/learning" of the device |deviceID| on the
  // computer |hostName|. Without a device ID, nothing is written.
  // |wallNow| gives the Unix time in seconds; without it, the system clock.
  LearnedVocabulary(const std::filesystem::path& dataFolder,
                    const std::string& deviceID, const std::string& hostName,
                    std::function<double()> wallNow = nullptr);
  ~LearnedVocabulary();

  [[nodiscard]] bool readOnly() const { return readOnly_; }
  [[nodiscard]] const std::filesystem::path& learningFolder() const {
    return learningFolder_;
  }

  // The entries of all the devices, read again now.
  //  English words: the word and the times typed, sorted by the lowercase
  //  word; the key is the word.
  //  Common sentences: the sentence, the times sent and the status, the most
  //  used first; the key is the sentence.
  //  Temporary phrases: the phrase, the reading and the date picked, the
  //  most recently picked first; the key is "<phrase> <reading>".
  std::vector<VocabularyRow> rows(LearnedKind kind,
                                  const SentenceStatusTexts& texts);

  // Adds the English word or the sentence typed, trimmed of spaces. Nothing
  // can be added to the temporary phrases. Gives kNothingTyped for an empty
  // field, kReadOnly without a device ID, kBadWord or kBadSentence when it
  // cannot be added, kFileMissing when this device's file cannot be loaded
  // yet (the folder may still be syncing), and kWriteFailed when a file
  // cannot be written.
  CommitResult add(LearnedKind kind, std::string_view field);

  // Removes the entries of |keys| on all the devices, recording them in
  // "removed.txt". All of them are removed together, writing removed.txt
  // once and this device's file once, or none is.
  BatchResult remove(LearnedKind kind, const std::vector<std::string>& keys);

  // Adds the temporary phrases of |keys| to the Chinese phrases (data.txt,
  // taking them out of exclude-phrases.txt), then forgets them. A phrase
  // already in data.txt is only forgotten. Nothing is done when data.txt
  // does not exist (kFileMissing). Stops writing data.txt at the first one
  // that fails; those written are then forgotten together (one write of
  // removed.txt and of this device's file), and count as done even if
  // forgetting them fails.
  BatchResult addToPhrases(const std::vector<std::string>& keys);

 private:
  // Why a store could not write: its own file has not loaded yet (the
  // folder may still be syncing), or a write failed.
  static DeleteStatus failure(bool pending);
  // Makes new stores, which read the files again when used.
  void rebuildStores();

  std::filesystem::path dataFolder_;
  std::filesystem::path learningFolder_;
  std::string deviceID_;
  std::string hostName_;
  bool readOnly_;
  std::function<double()> wallNow_;
  std::shared_ptr<SmartLearning::RemovedEntries> removedEntries_;
  std::unique_ptr<SmartLearning::EnglishWordStore> englishWords_;
  std::unique_ptr<SmartLearning::CommonSentenceStore> sentences_;
  std::unique_ptr<SmartLearning::TemporaryPhraseStore> temporaryPhrases_;
};

}  // namespace McBopomofo::VocabularyManager

#endif  // SRC_VOCABULARYMANAGER_LEARNEDVOCABULARY_H_
