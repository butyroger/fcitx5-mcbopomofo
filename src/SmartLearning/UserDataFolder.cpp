#include "UserDataFolder.h"

#include <cstdlib>
#include <system_error>
#include <utility>

namespace McBopomofo::SmartLearning {

namespace {

std::filesystem::path fallback(const std::filesystem::path& defaultFolder,
                               std::string* warning, std::string reason) {
  if (warning != nullptr) {
    *warning = std::move(reason);
  }
  return defaultFolder;
}

}  // namespace

std::filesystem::path resolveUserDataFolder(
    std::string_view configured, const std::filesystem::path& defaultFolder,
    std::string* warning) {
  if (warning != nullptr) {
    warning->clear();
  }
  std::error_code ec;

  if (configured.empty()) {
    if (!defaultFolder.empty() && !std::filesystem::exists(defaultFolder, ec)) {
      std::filesystem::create_directory(defaultFolder, ec);
    }
    return defaultFolder;
  }

  std::string text(configured);
  if (text == "~" || text.rfind("~/", 0) == 0) {
    const char* home = std::getenv("HOME");
    if (home == nullptr || *home == '\0') {
      return fallback(defaultFolder, warning,
                      "Cannot expand \"~\" because HOME is not set: " + text);
    }
    text = std::string(home) + text.substr(1);
  }

  std::filesystem::path path(text);
  if (!path.is_absolute()) {
    return fallback(defaultFolder, warning,
                    "The folder must be an absolute path: " + text);
  }
  // Drop a trailing separator so that parent_path() is the real parent.
  while (text.size() > 1 && text.back() == '/') {
    text.pop_back();
  }
  path = std::filesystem::path(text);

  if (std::filesystem::exists(path, ec)) {
    if (std::filesystem::is_directory(path, ec)) {
      return path;
    }
    return fallback(defaultFolder, warning,
                    "The path is not a folder: " + text);
  }

  std::filesystem::path parent = path.parent_path();
  if (parent.empty() || !std::filesystem::is_directory(parent, ec)) {
    return fallback(defaultFolder, warning,
                    "The parent folder does not exist: " + text);
  }
  if (!std::filesystem::create_directory(path, ec) || ec) {
    return fallback(defaultFolder, warning,
                    "Cannot create the folder: " + text);
  }
  return path;
}

bool shouldCreateTemplateFiles(const std::filesystem::path& folder,
                               const std::filesystem::path& defaultFolder) {
  return !folder.empty() &&
         (folder / "").lexically_normal() ==
             (defaultFolder / "").lexically_normal();
}

}  // namespace McBopomofo::SmartLearning
