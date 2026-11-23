#include "ManagerSettings.h"

#include <cstdlib>

#include "AtomicFile.h"
#include "LearningPaths.h"
#include "UserDataFolder.h"

namespace McBopomofo::VocabularyManager {

namespace fs = std::filesystem;

namespace {

constexpr char kDataFolderOption[] = "--data-folder";
constexpr char kDeviceIDOption[] = "--device-id";
constexpr char kSelfTestOption[] = "--self-test";
constexpr char kBuiltInData[] = "data/mcbopomofo-data.txt";

std::string_view trimAscii(std::string_view text) {
  constexpr std::string_view kSpaces = " \t\r\n\f\v";
  size_t begin = text.find_first_not_of(kSpaces);
  if (begin == std::string_view::npos) {
    return {};
  }
  size_t end = text.find_last_not_of(kSpaces);
  return text.substr(begin, end - begin + 1);
}

std::string unquote(std::string_view value) {
  if (value.size() < 2 || value.front() != '"' || value.back() != '"') {
    return std::string(value);
  }
  value = value.substr(1, value.size() - 2);
  std::string result;
  for (size_t i = 0; i < value.size(); ++i) {
    char c = value[i];
    if (c == '\\' && i + 1 < value.size()) {
      char next = value[++i];
      result += next == 'n' ? '\n' : next;
    } else {
      result += c;
    }
  }
  return result;
}

// $<variable>, or $HOME/<fallback> when it is not set or not absolute.
fs::path xdgFolder(const GetEnv& env, const char* variable,
                   const char* fallback) {
  if (auto value = env(variable); value && !value->empty() &&
                                  fs::path(*value).is_absolute()) {
    return fs::path(*value);
  }
  return fs::path(env("HOME").value_or("")) / fallback;
}

}  // namespace

GetEnv processEnvironment() {
  return [](const char* name) -> std::optional<std::string> {
    const char* value = std::getenv(name);
    if (value == nullptr) {
      return std::nullopt;
    }
    return std::string(value);
  };
}

LaunchOptions parseArguments(const std::vector<std::string>& arguments) {
  LaunchOptions options;
  for (size_t i = 0; i < arguments.size(); ++i) {
    const std::string& argument = arguments[i];
    if (argument == kSelfTestOption) {
      options.selfTest = true;
      continue;
    }
    std::optional<std::string>* target = nullptr;
    std::string name = argument;
    std::optional<std::string> value;
    if (size_t equal = argument.find('='); equal != std::string::npos) {
      name = argument.substr(0, equal);
      value = argument.substr(equal + 1);
    }
    if (name == kDataFolderOption) {
      target = &options.dataFolder;
    } else if (name == kDeviceIDOption) {
      target = &options.deviceID;
    } else {
      options.error = argument;
      return options;
    }
    if (!value) {
      if (i + 1 >= arguments.size()) {
        options.error = argument;
        return options;
      }
      value = arguments[++i];
    }
    *target = std::move(value);
  }
  return options;
}

InputMethodConfig parseInputMethodConfig(std::string_view content) {
  InputMethodConfig config;
  size_t start = 0;
  while (start <= content.size()) {
    size_t end = content.find('\n', start);
    if (end == std::string_view::npos) {
      end = content.size();
    }
    std::string_view line = trimAscii(content.substr(start, end - start));
    start = end + 1;
    if (line.empty() || line.front() == '#') {
      continue;
    }
    if (line.front() == '[') {
      // The keys of a group are not the ones looked for.
      break;
    }
    size_t equal = line.find('=');
    if (equal == std::string_view::npos) {
      continue;
    }
    std::string_view key = trimAscii(line.substr(0, equal));
    std::string value = unquote(trimAscii(line.substr(equal + 1)));
    if (key == "UserDataFolder") {
      config.userDataFolder = std::move(value);
    } else if (key == "DeviceID") {
      config.deviceID = std::move(value);
    }
  }
  return config;
}

fs::path inputMethodConfigFile(const GetEnv& env) {
  return xdgFolder(env, "XDG_CONFIG_HOME", ".config") / "fcitx5" / "conf" /
         "mcbopomofo.conf";
}

fs::path defaultUserDataFolder(const GetEnv& env) {
  return xdgFolder(env, "XDG_DATA_HOME", ".local/share") / "fcitx5" /
         "mcbopomofo";
}

std::vector<fs::path> builtInDataCandidates(
    const GetEnv& env, const fs::path& installedPkgDataFolder) {
  std::vector<fs::path> candidates;
  candidates.push_back(xdgFolder(env, "XDG_DATA_HOME", ".local/share") /
                       "fcitx5" / kBuiltInData);
  std::string dirs = env("XDG_DATA_DIRS").value_or("");
  if (dirs.empty()) {
    dirs = "/usr/local/share:/usr/share";
  }
  size_t start = 0;
  while (start <= dirs.size()) {
    size_t end = dirs.find(':', start);
    if (end == std::string::npos) {
      end = dirs.size();
    }
    fs::path dir(dirs.substr(start, end - start));
    start = end + 1;
    if (dir.empty() || !dir.is_absolute()) {
      continue;
    }
    candidates.push_back(dir / "fcitx5" / kBuiltInData);
  }
  if (!installedPkgDataFolder.empty()) {
    candidates.push_back(installedPkgDataFolder / kBuiltInData);
  }
  // "/b/" and "/b" give the same file.
  for (auto& candidate : candidates) {
    candidate = candidate.lexically_normal();
  }
  return candidates;
}

fs::path findBuiltInData(const GetEnv& env,
                         const fs::path& installedPkgDataFolder) {
  for (const auto& candidate :
       builtInDataCandidates(env, installedPkgDataFolder)) {
    std::error_code ec;
    if (fs::is_regular_file(candidate, ec)) {
      return candidate;
    }
  }
  return {};
}

Settings resolveSettings(const LaunchOptions& options, const GetEnv& env,
                         std::string_view hostName) {
  Settings settings;
  InputMethodConfig config;
  if (!options.dataFolder || !options.deviceID) {
    if (auto content =
            SmartLearning::readWholeFile(inputMethodConfigFile(env))) {
      config = parseInputMethodConfig(*content);
    }
  }
  if (options.dataFolder && !options.dataFolder->empty()) {
    // The input method passes the folder it resolved.
    settings.dataFolder = *options.dataFolder;
  } else {
    settings.dataFolder = SmartLearning::resolveUserDataFolder(
        config.userDataFolder, defaultUserDataFolder(env), &settings.warning);
  }
  settings.deviceID = options.deviceID ? *options.deviceID : config.deviceID;
  settings.hostName = std::string(hostName);
  settings.readOnly = settings.deviceID.empty();
  if (!settings.readOnly) {
    settings.ownLearningFilename = SmartLearning::learningFilename(
        settings.deviceID, SmartLearning::sanitizeDeviceName(hostName));
  }
  return settings;
}

std::string encodeLaunchMessage(const std::vector<std::string>& arguments) {
  std::string message;
  for (const auto& argument : arguments) {
    message += argument;
    message += '\0';
  }
  return message;
}

std::vector<std::string> decodeLaunchMessage(std::string_view message) {
  std::vector<std::string> arguments;
  size_t start = 0;
  while (true) {
    size_t end = message.find('\0', start);
    if (end == std::string_view::npos) {
      break;
    }
    arguments.emplace_back(message.substr(start, end - start));
    start = end + 1;
  }
  return arguments;
}

std::optional<Settings> adoptLaunch(const Settings& current,
                                    const LaunchOptions& options,
                                    const GetEnv& env,
                                    std::string_view hostName) {
  if (!options.deviceID || options.deviceID->empty()) {
    return std::nullopt;
  }
  if (current.readOnly) {
    return resolveSettings(options, env, hostName);
  }
  // The input method of the same device now uses another folder.
  if (*options.deviceID == current.deviceID && options.dataFolder &&
      !options.dataFolder->empty() &&
      std::filesystem::path(*options.dataFolder) != current.dataFolder) {
    return resolveSettings(options, env, hostName);
  }
  return std::nullopt;
}

}  // namespace McBopomofo::VocabularyManager
