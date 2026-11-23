#ifndef SRC_SMARTLEARNING_ATOMICFILE_H_
#define SRC_SMARTLEARNING_ATOMICFILE_H_

#include <filesystem>
#include <string_view>

namespace McBopomofo::SmartLearning {

// Writes |content| to a temporary file ".<name>.tmp-<pid>" next to |path| and
// renames it over |path|. Returns false on any failure; the temporary file is
// removed in that case.
bool writeFileAtomically(const std::filesystem::path& path,
                         std::string_view content);

}  // namespace McBopomofo::SmartLearning

#endif  // SRC_SMARTLEARNING_ATOMICFILE_H_
