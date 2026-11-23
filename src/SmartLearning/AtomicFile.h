#ifndef SRC_SMARTLEARNING_ATOMICFILE_H_
#define SRC_SMARTLEARNING_ATOMICFILE_H_

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

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

}  // namespace McBopomofo::SmartLearning

#endif  // SRC_SMARTLEARNING_ATOMICFILE_H_
