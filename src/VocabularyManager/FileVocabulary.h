#ifndef SRC_VOCABULARYMANAGER_FILEVOCABULARY_H_
#define SRC_VOCABULARYMANAGER_FILEVOCABULARY_H_

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace McBopomofo {
class McBopomofoLM;
}

// The tabs of the vocabulary manager that list the lines of one file:
// the Chinese phrases (data.txt), the excluded phrases (exclude-phrases.txt)
// and the text templates (text-templates.txt). Nothing here depends on Qt.
namespace McBopomofo::VocabularyManager {

enum class FileKind { kPhrases, kExcluded, kTemplates };

// The file of the kind in the user data folder.
std::filesystem::path vocabularyFile(const std::filesystem::path& folder,
                                     FileKind kind);

// One line of the list.
struct VocabularyRow {
  // Unique in the list: the line number among the listed lines, a tab, and
  // the line.
  std::string id;
  std::vector<std::string> columns;
  // The line in the file.
  std::string key;
};

// The lines of the file listed, in the order of the file. Lines split at
// "\n"; blank lines and lines starting with "#" are skipped. A phrase line is
// split once at a space (spaces at the start are skipped) and is listed when
// it gives two columns. A template line is split at the first tab.
std::vector<VocabularyRow> parseRows(FileKind kind, std::string_view content);

// parseRows() of the file; no rows when it is missing or cannot be read.
std::vector<VocabularyRow> loadRows(FileKind kind,
                                    const std::filesystem::path& file);

// |text| without the spaces and tabs at both ends (the Unicode space
// separators included; line breaks are kept).
std::string trimSpaces(std::string_view text);

enum class CommitStatus {
  // The first field is empty; nothing is done.
  kNothingTyped,
  // A phrase has no reading, a reading with "？", or a column that would
  // break the line.
  kBadReading,
  // A new phrase that is already a line of the file.
  kAlreadyListed,
  // A template has no text, or a tab or a line break.
  kBadTemplate,
  kAdded,
  kSaved,
  // The file does not exist (the folder may still be syncing); it is not
  // created.
  kFileMissing,
  kWriteFailed,
};

struct CommitResult {
  CommitStatus status;
  // The first field, trimmed.
  std::string value;
};

// Adds the entry typed in the two fields, or saves the line |editingKey|
// being edited: the first line that is exactly |editingKey| is replaced;
// when there is none, the entry is added to the end of the file. The fields
// are trimmed of spaces first.
CommitResult commitEntry(FileKind kind, const std::filesystem::path& file,
                         std::string_view field1, std::string_view field2,
                         const std::optional<std::string>& editingKey);

enum class DeleteStatus { kNothingSelected, kDeleted, kFileMissing, kWriteFailed };

// Removes one line of the file for each of |keys|.
DeleteStatus deleteEntries(const std::filesystem::path& file,
                           const std::vector<std::string>& keys);

// Fills in the reading of a phrase from the built-in phrase data, which is
// loaded the first time a reading is asked for.
class ReadingFiller {
 public:
  explicit ReadingFiller(std::filesystem::path dataFile);
  ~ReadingFiller();

  // The reading, made as ExtractReading() does, of |phrase| trimmed of
  // spaces. Empty when the phrase is empty, nothing in it is found, or the
  // data cannot be loaded.
  std::string readingFor(std::string_view phrase);

 private:
  std::filesystem::path dataFile_;
  bool loaded_ = false;
  std::unique_ptr<McBopomofoLM> lm_;
};

}  // namespace McBopomofo::VocabularyManager

#endif  // SRC_VOCABULARYMANAGER_FILEVOCABULARY_H_
