#ifndef SRC_SMARTLEARNING_USERPHRASEFILE_H_
#define SRC_SMARTLEARNING_USERPHRASEFILE_H_

#include <filesystem>
#include <string>

// Edits of the user phrase file (data.txt) and the excluded phrase file
// (exclude-phrases.txt). Each line of both files is "<value> <reading>".
//
// A line holds a phrase when, with the white space at both ends trimmed, it
// splits on single spaces into exactly two columns: the value and the
// reading. Other lines (comments, lines with tabs or with more than one
// space between the columns) are kept as they are.
//
// Every edit reads the file again right before changing it (it may be
// shared and synced), and writes the whole file back atomically. A file
// that cannot be read is never written. The value and the reading must be
// non-empty and must not contain a space, a tab or a line break.
namespace McBopomofo::SmartLearning::UserPhraseFile {

// Learns a phrase picked often enough. Among the user phrases of the same
// reading, the earlier one wins, so the phrase goes right before the first
// phrase of the same reading (its other lines are removed), or to the end of
// the file if no phrase has the reading. A phrase in the excluded file (or
// when the excluded file exists but cannot be read) is never learned.
// Returns false, changing nothing, if the phrase is excluded, is already the
// first one of its reading, or the data file is missing or cannot be read
// or written.
bool learn(const std::filesystem::path& data,
           const std::filesystem::path& exclude, const std::string& value,
           const std::string& reading);

// Boosts a phrase: takes it out of the excluded file, then adds it to the
// end of the data file. Returns false if the data file already has it, or
// if the data file is missing or cannot be read or written; nothing is
// written at all when the data file cannot be read.
bool write(const std::filesystem::path& data,
           const std::filesystem::path& exclude, const std::string& value,
           const std::string& reading);

// Excludes a phrase: takes it out of the data file, then adds it to the end
// of the excluded file. Returns false if the excluded file already has it,
// or if the excluded file is missing or cannot be read or written; nothing
// is written at all when the excluded file cannot be read.
bool remove(const std::filesystem::path& data,
            const std::filesystem::path& exclude, const std::string& value,
            const std::string& reading);

}  // namespace McBopomofo::SmartLearning::UserPhraseFile

#endif  // SRC_SMARTLEARNING_USERPHRASEFILE_H_
