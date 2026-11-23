#ifndef SRC_SMARTLEARNING_USERDATAFOLDER_H_
#define SRC_SMARTLEARNING_USERDATAFOLDER_H_

#include <filesystem>
#include <string>
#include <string_view>

namespace McBopomofo::SmartLearning {

// Resolves the folder that holds the user's phrase files.
//
// - An empty |configured| means |defaultFolder| (created if missing).
// - A leading "~/" is expanded to $HOME.
// - An existing directory is returned as is.
// - A missing directory whose parent exists is created (only that one level).
// - Anything else (missing parent, cannot be created, a regular file, a
//   relative path) returns |defaultFolder| and sets |*warning| to the reason.
//   The configured string is never altered by this function.
std::filesystem::path resolveUserDataFolder(
    std::string_view configured, const std::filesystem::path& defaultFolder,
    std::string* warning);

}  // namespace McBopomofo::SmartLearning

#endif  // SRC_SMARTLEARNING_USERDATAFOLDER_H_
