#ifndef SRC_SMARTLEARNING_ENGLISHWORDS_H_
#define SRC_SMARTLEARNING_ENGLISHWORDS_H_

#include <cstddef>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "RemovedEntries.h"

namespace McBopomofo::SmartLearning {

// A read-only list of English words, kept sorted and in lowercase, for
// spelling checks and prefix lookups.
class EnglishLexicon {
 public:
  // Reads one word per line. Only lines made entirely of ASCII letters are
  // kept (so possessives such as "dog's" are dropped), in lowercase, sorted
  // and without duplicates. A missing or unreadable file gives an empty list.
  static std::shared_ptr<EnglishLexicon> loadFromFile(
      const std::filesystem::path& path);

  // The same filtering as loadFromFile, from words in memory.
  static std::shared_ptr<EnglishLexicon> fromWords(
      std::vector<std::string> words);

  bool contains(std::string_view lowercased) const;

  // True if a word longer than |lowercased| starts with it, such as
  // "helpful" for "helpf".
  bool hasLongerWordWithPrefix(std::string_view lowercased) const;

  // At most |limit| words that start with |lowercased| and are longer than
  // it, the shorter ones first, and words of the same length in alphabetical
  // order.
  std::vector<std::string> wordsWithPrefix(std::string_view lowercased,
                                           size_t limit) const;

  [[nodiscard]] size_t size() const { return words_.size(); }

 private:
  explicit EnglishLexicon(std::vector<std::string> words)
      : words_(std::move(words)) {}
  std::vector<std::string> words_;
};

inline constexpr char kDefaultEnglishWordList[] =
    "/usr/share/dict/american-english";

// The English words the user has typed, per device, in the "english" folder
// of the shared learning folder, together with a word list and well-known
// words, to tell whether some letters are an English word and in which case
// to write it.
//
// Each device writes only its own "<deviceID>-<deviceName>.txt" (one
// "word<TAB>count" per line); the counts of the other devices' files are
// added on reading, at most once a minute. Words the user removed (the
// "english" entries of "removed.txt" in the learning folder) are left out.
//
// The own file is read only once, and is never written before it has been
// read: until then (the "english" folder does not exist yet, or the file
// cannot be read) the store is pending, and the words recorded meanwhile are
// added to the file's counts once it loads.
class EnglishWordStore {
 public:
  static constexpr size_t kMinimumLength = 3;
  static constexpr double kReloadInterval = 60;  // seconds
  static constexpr size_t kMaximumCompletions = 20;

  // Without |removedEntries|, the store reads "removed.txt" with its own
  // reader, using |now|.
  EnglishWordStore(std::string deviceID, std::string deviceName,
                   std::shared_ptr<const EnglishLexicon> lexicon,
                   std::function<double()> now,
                   std::shared_ptr<RemovedEntries> removedEntries = nullptr);

  // Uses "<learningFolder>/english" and "<learningFolder>/removed.txt". An
  // empty or relative path is ignored. Setting the current folder again only
  // reloads. When switching folders, the own words are saved to the old
  // folder (if its file was loaded), then replaced by what the new folder's
  // own file holds.
  void setLearningFolder(std::filesystem::path learningFolder);

  // Counts the words of at least kMinimumLength characters.
  void record(const std::vector<std::string>& words);

  // Writes the own words to the own file atomically. Returns false, writing
  // nothing, while the own file is pending or if the write fails.
  bool save();

  [[nodiscard]] bool hasUnsavedChanges() const { return unsavedChanges_; }

  // The case to use for a word typed in lowercase, like "iPhone" for
  // "iphone": the mixed-case form typed most often (at least twice), or a
  // well-known one.
  std::optional<std::string> preferredCase(const std::string& word);

  // Whether the text is an English word: an abbreviation in uppercase (like
  // PDF and ChatGPT), a well-known word, a word typed twice or more, or a
  // word in the word list. Letters typed after an abbreviation, like
  // "PDFdkf", are not a word.
  bool isWord(const std::string& word);

  // Whether the text is an English word, or the beginning of one, such as
  // "helpf" for helpful.
  bool isWordOrPrefix(const std::string& text);

  // The words to offer for |word| (of two letters or more), at most
  // kMaximumCompletions, without |word| itself:
  //  1. words typed twice or more, the most typed first, that start with the
  //     word, then those that have its letters in order and the same first
  //     letter (such as "associated" for "aocd");
  //  2. words of the word list that start with the word;
  //  3. the words typed once found as in 1, which may be typos.
  // Only single words of ASCII letters are offered. When the word starts with
  // an uppercase letter, the lowercase words offered do as well.
  std::vector<std::string> completions(const std::string& word);

  [[nodiscard]] bool isLocalFilePending() const { return localFilePending_; }

 private:
  void loadIfNeeded();
  void reload();
  // Returns true if the own file was loaded (or found absent) by this call.
  bool loadLocalFileIfNeeded();
  std::filesystem::path englishFolder() const;
  std::filesystem::path ownFilePath() const;

  std::string deviceID_;
  std::string ownFilename_;
  std::shared_ptr<const EnglishLexicon> lexicon_;
  std::function<double()> now_;
  std::shared_ptr<RemovedEntries> removedEntries_;

  std::filesystem::path learningFolder_;
  bool hasFolder_ = false;
  bool localFilePending_ = true;
  bool unsavedChanges_ = false;
  bool loaded_ = false;
  double lastLoaded_ = 0;

  // The counts of the words typed on this device, and on all devices.
  std::map<std::string, long long> localCounts_;
  std::map<std::string, long long> counts_;
};

}  // namespace McBopomofo::SmartLearning

#endif  // SRC_SMARTLEARNING_ENGLISHWORDS_H_
