#include "LearningFileUtils.h"

#include <charconv>
#include <chrono>
#include <system_error>

namespace McBopomofo::SmartLearning {
namespace fs = std::filesystem;

namespace {

// An optional sign and the number, with nothing else.
template <typename T>
std::optional<T> parseWhole(std::string_view s) {
  if (!s.empty() && s.front() == '+') {
    s.remove_prefix(1);
    if (!s.empty() && s.front() == '-') {
      return std::nullopt;
    }
  }
  T value = 0;
  auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), value);
  if (s.empty() || ec != std::errc() || ptr != s.data() + s.size()) {
    return std::nullopt;
  }
  return value;
}

}  // namespace

fs::path normalizedFolder(const fs::path& path) {
  fs::path normal = path.lexically_normal();
  if (normal.filename().empty() && normal.has_parent_path()) {
    normal = normal.parent_path();
  }
  return normal;
}

std::vector<std::string_view> split(std::string_view s, char separator,
                                    bool keepEmpty) {
  std::vector<std::string_view> parts;
  size_t start = 0;
  while (true) {
    size_t end = s.find(separator, start);
    std::string_view part = s.substr(
        start, end == std::string_view::npos ? std::string_view::npos
                                             : end - start);
    if (keepEmpty || !part.empty()) {
      parts.push_back(part);
    }
    if (end == std::string_view::npos) {
      break;
    }
    start = end + 1;
  }
  return parts;
}

std::optional<long long> parseInteger(std::string_view s) {
  return parseWhole<long long>(s);
}

std::optional<double> parseNumber(std::string_view s) {
  return parseWhole<double>(s);
}

bool isPlainPart(const std::string& s) {
  return !s.empty() && s.find_first_of(" \t\r\n") == std::string::npos;
}

double steadySeconds() {
  return std::chrono::duration<double>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

FileStamps stampsOf(const std::vector<fs::path>& paths) {
  FileStamps stamps;
  for (const auto& path : paths) {
    FileStamp stamp = stampOf(path);
    if (stamp.exists) {
      stamps[path.filename().string()] = stamp;
    }
  }
  return stamps;
}

FileStamps txtFileStamps(const fs::path& folder) {
  std::vector<fs::path> paths;
  std::error_code ec;
  fs::directory_iterator it(folder, ec);
  if (ec) {
    return {};
  }
  for (const fs::directory_iterator end; it != end; it.increment(ec)) {
    if (ec) {
      break;
    }
    if (it->path().extension() == ".txt") {
      paths.push_back(it->path());
    }
  }
  // stampOf() leaves out what is not a regular file.
  return stampsOf(paths);
}

}  // namespace McBopomofo::SmartLearning
