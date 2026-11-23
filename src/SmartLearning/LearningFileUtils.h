#ifndef SRC_SMARTLEARNING_LEARNINGFILEUTILS_H_
#define SRC_SMARTLEARNING_LEARNINGFILEUTILS_H_

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "AtomicFile.h"

// Small helpers shared by the learning stores, for reading their files and
// telling when the files changed.
namespace McBopomofo::SmartLearning {

// "a/./b/" and "a/b" are the same folder.
std::filesystem::path normalizedFolder(const std::filesystem::path& path);

// Splits on |separator|. Empty parts are dropped unless |keepEmpty|.
std::vector<std::string_view> split(std::string_view s, char separator,
                                    bool keepEmpty);

// An integer with an optional sign, and nothing else.
std::optional<long long> parseInteger(std::string_view s);

// A decimal number with an optional sign, and nothing else.
std::optional<double> parseNumber(std::string_view s);

// Not empty, and without a space, a tab or a line break, which would break
// the "value reading" line format.
bool isPlainPart(const std::string& s);

// The time of a steady clock, in seconds, to tell when to read a file again.
double steadySeconds();

// The stamps of files, by file name.
using FileStamps = std::map<std::string, FileStamp>;

// The stamps of the files that exist among |paths|.
FileStamps stampsOf(const std::vector<std::filesystem::path>& paths);

// The stamps of the regular ".txt" files directly inside |folder|, hidden
// files and sync conflict copies included. Empty if the folder cannot be
// read.
FileStamps txtFileStamps(const std::filesystem::path& folder);

}  // namespace McBopomofo::SmartLearning

#endif  // SRC_SMARTLEARNING_LEARNINGFILEUTILS_H_
