#include "RemovedEntries.h"

#include <charconv>
#include <optional>
#include <system_error>
#include <utility>

#include "AtomicFile.h"

namespace McBopomofo::SmartLearning {
namespace fs = std::filesystem;

namespace {

constexpr char kRemovedFilename[] = "removed.txt";

// A decimal number with an optional sign, and nothing else.
std::optional<double> parseNumber(std::string_view s) {
  if (!s.empty() && s.front() == '+') {
    s.remove_prefix(1);
    if (!s.empty() && s.front() == '-') {
      return std::nullopt;
    }
  }
  double value = 0;
  auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), value);
  if (s.empty() || ec != std::errc() || ptr != s.data() + s.size()) {
    return std::nullopt;
  }
  return value;
}

std::string entryKey(std::string_view kind, std::string_view key) {
  std::string result(kind);
  result += '\t';
  result += key;
  return result;
}

}  // namespace

RemovedEntries::RemovedEntries(std::function<double()> now)
    : now_(std::move(now)) {}

void RemovedEntries::setLearningFolder(fs::path learningFolder) {
  // A relative path would resolve against the process's working directory.
  if (learningFolder.empty() || !learningFolder.is_absolute()) {
    return;
  }
  if (learningFolder.lexically_normal() != learningFolder_.lexically_normal()) {
    learningFolder_ = std::move(learningFolder);
    loaded_ = false;
  }
}

double RemovedEntries::removedTime(std::string_view kind,
                                   std::string_view key) {
  loadIfNeeded();
  auto it = entries_.find(entryKey(kind, key));
  return it == entries_.end() ? 0 : it->second;
}

void RemovedEntries::loadIfNeeded() {
  if (loaded_ && now_() - lastLoaded_ < kReloadInterval) {
    return;
  }
  reload();
}

void RemovedEntries::reload() {
  std::unordered_map<std::string, double> entries;
  if (!learningFolder_.empty()) {
    if (auto content = readWholeFile(learningFolder_ / kRemovedFilename)) {
      std::string_view rest(*content);
      while (!rest.empty()) {
        size_t end = rest.find('\n');
        std::string_view line = rest.substr(0, end);
        rest = end == std::string_view::npos ? std::string_view()
                                             : rest.substr(end + 1);
        // Exactly three columns, empty ones included.
        size_t firstTab = line.find('\t');
        if (firstTab == std::string_view::npos) {
          continue;
        }
        size_t secondTab = line.find('\t', firstTab + 1);
        if (secondTab == std::string_view::npos ||
            line.find('\t', secondTab + 1) != std::string_view::npos) {
          continue;
        }
        if (auto time = parseNumber(line.substr(secondTab + 1))) {
          entries[std::string(line.substr(0, secondTab))] = *time;
        }
      }
    }
  }
  entries_ = std::move(entries);
  loaded_ = true;
  lastLoaded_ = now_();
}

}  // namespace McBopomofo::SmartLearning
