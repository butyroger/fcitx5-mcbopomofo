#include "LearningPaths.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <random>
#include <system_error>

namespace McBopomofo::SmartLearning {
namespace fs = std::filesystem;

namespace {

// Lists regular ".txt" files directly inside |folder|, sorted by file name.
// Hidden files (leading '.') and sync conflict copies are left out.
std::vector<fs::path> listCandidates(const fs::path& folder) {
  std::vector<fs::path> result;
  std::error_code ec;
  fs::directory_iterator it(folder, ec);
  if (ec) {
    return result;
  }
  for (const fs::directory_iterator end; it != end; it.increment(ec)) {
    if (ec) {
      return {};
    }
    const fs::path& path = it->path();
    std::string name = path.filename().string();
    if (name.empty() || name[0] == '.' || path.extension() != ".txt" ||
        isSyncConflictCopy(name)) {
      continue;
    }
    std::error_code statusEc;
    if (!it->is_regular_file(statusEc) || statusEc) {
      continue;
    }
    result.push_back(path);
  }
  std::sort(result.begin(), result.end(),
            [](const fs::path& a, const fs::path& b) {
              return a.filename().string() < b.filename().string();
            });
  return result;
}

}  // namespace

std::string newDeviceID() {
  std::random_device rd;
  uint32_t value = static_cast<uint32_t>(rd());
  char buf[9];
  std::snprintf(buf, sizeof(buf), "%08X", static_cast<unsigned>(value));
  return std::string(buf);
}

std::string sanitizeDeviceName(std::string_view name) {
  std::string result(name);
  for (char& c : result) {
    if (c == '/' || c == ':' || c == '\\' || c == '\n' || c == '\r' ||
        c == '\t') {
      c = '_';
    }
  }
  if (result.empty()) {
    return "Linux";
  }
  return result;
}

std::string learningFilename(std::string_view deviceID,
                             std::string_view deviceName) {
  std::string result(deviceID);
  result += '-';
  result += deviceName;
  result += ".txt";
  return result;
}

bool isSyncConflictCopy(std::string_view filename) {
  return filename.find("_Conflict") != std::string_view::npos ||
         filename.find(".sync-conflict-") != std::string_view::npos;
}

std::vector<fs::path> otherDevicesLearningFiles(const fs::path& folder,
                                                std::string_view ownFilename) {
  std::vector<fs::path> result;
  for (auto& path : listCandidates(folder)) {
    if (path.filename().string() == ownFilename) {
      continue;
    }
    result.push_back(std::move(path));
  }
  return result;
}

std::optional<fs::path> findRenamedOwnFile(const fs::path& folder,
                                           std::string_view deviceID,
                                           std::string_view ownFilename) {
  std::string prefix(deviceID);
  prefix += '-';
  for (auto& path : listCandidates(folder)) {
    std::string name = path.filename().string();
    if (name == ownFilename) {
      continue;
    }
    if (name.compare(0, prefix.size(), prefix) == 0) {
      return std::move(path);
    }
  }
  return std::nullopt;
}

}  // namespace McBopomofo::SmartLearning
