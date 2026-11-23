#ifndef SRC_SMARTLEARNING_VOCABULARYFILE_H_
#define SRC_SMARTLEARNING_VOCABULARYFILE_H_

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace McBopomofo::SmartLearning::VocabularyFile {

// Line edits of the single files that people also edit by hand (data.txt,
// exclude-phrases.txt, text-templates.txt). Comments, blank lines and the
// order of the other lines are kept as they are. Lines are split at "\n"
// only, and compared as they are, so a trailing space or "\r" counts.
//
// Nothing is written, and false is returned, if the file does not exist (the
// folder may still be syncing) or cannot be read. The file is written with
// writeFileAtomically().

// Replaces the first line that is exactly |oldLine| with |newLine|. If there
// is no |oldLine|, or it is not found, |newLine| is added at the end of the
// file. A line replaced by the same line writes nothing.
bool replaceOrAppendLine(const std::filesystem::path& path,
                         const std::string& newLine,
                         const std::optional<std::string>& oldLine);

// Removes, for each of |lines|, the first line that is exactly the same. A
// line that appears twice in the file but once in |lines| loses one line.
// Returns true also if nothing needed to be removed; nothing is written then.
bool removeLines(const std::filesystem::path& path,
                 const std::vector<std::string>& lines);

}  // namespace McBopomofo::SmartLearning::VocabularyFile

#endif  // SRC_SMARTLEARNING_VOCABULARYFILE_H_
