#ifndef SRC_VOCABULARYMANAGER_MANAGERSETTINGS_H_
#define SRC_VOCABULARYMANAGER_MANAGERSETTINGS_H_

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// Where the vocabulary manager finds the user's files, and as which device it
// writes. Nothing here depends on Qt.
namespace McBopomofo::VocabularyManager {

// Reads an environment variable; nullopt when it is not set.
using GetEnv = std::function<std::optional<std::string>(const char*)>;

// The environment of the process.
GetEnv processEnvironment();

// The command line, without the program name.
//   --data-folder <folder>   the folder holding data.txt and the other files
//   --device-id <id>         the device ID of the input method
//   --self-test              opens the window, loads every tab and quits
// "--name=value" is also accepted.
struct LaunchOptions {
  std::optional<std::string> dataFolder;
  std::optional<std::string> deviceID;
  bool selfTest = false;
  // Set to the offending argument when the command line cannot be used.
  std::string error;
};

LaunchOptions parseArguments(const std::vector<std::string>& arguments);

// The values the manager needs from the input method's config file.
struct InputMethodConfig {
  std::string userDataFolder;
  std::string deviceID;
};

// Parses the input method's config file (an INI file written by fcitx).
// Only keys before the first [group] are looked at. A value in double quotes
// is unquoted, with \\, \n and \" unescaped.
InputMethodConfig parseInputMethodConfig(std::string_view content);

// $XDG_CONFIG_HOME/fcitx5/conf/mcbopomofo.conf ($HOME/.config by default).
std::filesystem::path inputMethodConfigFile(const GetEnv& env);

// $XDG_DATA_HOME/fcitx5/mcbopomofo ($HOME/.local/share by default), the
// folder the input method uses when none is chosen.
std::filesystem::path defaultUserDataFolder(const GetEnv& env);

// The places of the built-in phrase data, in the order the input method
// looks for them: $XDG_DATA_HOME/fcitx5/data, each folder of $XDG_DATA_DIRS
// (/usr/local/share:/usr/share by default) followed by fcitx5/data, and
// then |installedPkgDataFolder|/data.
std::vector<std::filesystem::path> builtInDataCandidates(
    const GetEnv& env, const std::filesystem::path& installedPkgDataFolder);

// The first of builtInDataCandidates() that is a file; empty if none is.
std::filesystem::path findBuiltInData(
    const GetEnv& env, const std::filesystem::path& installedPkgDataFolder);

struct Settings {
  std::filesystem::path dataFolder;
  std::string deviceID;
  // Without a device ID (the input method has never run), nothing is
  // written.
  bool readOnly = true;
  // This device's file name in the learning folders, the same as the input
  // method's. Empty when read only.
  std::string ownLearningFilename;
  // Why the configured folder is not used, if it is not.
  std::string warning;
};

// The folder and the device ID come from the command line; what is not given
// there comes from the input method's config file. A folder from the config
// file is resolved as the input method does (resolveUserDataFolder()).
Settings resolveSettings(const LaunchOptions& options, const GetEnv& env,
                         std::string_view hostName);

}  // namespace McBopomofo::VocabularyManager

#endif  // SRC_VOCABULARYMANAGER_MANAGERSETTINGS_H_
