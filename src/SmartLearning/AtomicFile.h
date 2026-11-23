#ifndef SRC_SMARTLEARNING_ATOMICFILE_H_
#define SRC_SMARTLEARNING_ATOMICFILE_H_

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace McBopomofo::SmartLearning {

// Writes |content| to a temporary file ".<name>.tmp-<pid>" next to |path|,
// syncs it to disk, and renames it over |path| only if the write, the sync and
// the close all succeeded. Returns false on any failure; the temporary file is
// removed in that case and |path| is left as it was.
bool writeFileAtomically(const std::filesystem::path& path,
                         std::string_view content);

// Creates |path| with |content| only if it does not exist yet. The file is
// opened with O_CREAT|O_EXCL, so an existing file (for example one that a
// sync client just put there) is never truncated. Returns true if the file was
// created; false if it already existed or on any error.
bool createFileIfAbsent(const std::filesystem::path& path,
                        std::string_view content);

// Reads the whole file. Returns nullopt if it is not a regular file or
// cannot be read completely.
std::optional<std::string> readWholeFile(const std::filesystem::path& path);

// What tells whether a file changed: whether it exists, its modification time
// and its size.
struct FileStamp {
  bool exists = false;
  std::filesystem::file_time_type modified{};
  std::uintmax_t size = 0;

  bool operator==(const FileStamp& other) const {
    return exists == other.exists && modified == other.modified &&
           size == other.size;
  }
  bool operator!=(const FileStamp& other) const { return !(*this == other); }
};

FileStamp stampOf(const std::filesystem::path& path);

// What readIfChanged() found.
enum class FileChange {
  // The file has the stamp it had, or it is gone (which removes nothing).
  kUnchanged,
  // The file changed and was read.
  kChanged,
  // The file changed but cannot be read.
  kUnreadable,
};

// Reads the file at |path| if its stamp is not |*stamp|, for a store to merge
// what another program wrote to its own file. |*stamp| is updated unless the
// file cannot be read; |*content| is set only for kChanged.
FileChange readIfChanged(const std::filesystem::path& path, FileStamp* stamp,
                         std::string* content);

// The lines of a text, split at any line break: LF, CR, VT, FF, NEL, LINE
// SEPARATOR and PARAGRAPH SEPARATOR. CR LF gives an empty line between, like
// a split at each of them.
std::vector<std::string_view> splitLines(std::string_view text);

}  // namespace McBopomofo::SmartLearning

#endif  // SRC_SMARTLEARNING_ATOMICFILE_H_
