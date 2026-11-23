#include "AtomicFile.h"

#include <fcntl.h>
#include <unistd.h>

#include <cerrno>
#include <string>
#include <system_error>

namespace McBopomofo::SmartLearning {
namespace fs = std::filesystem;

namespace {

// Writes all of |content| to |fd|, retrying when interrupted by a signal.
bool writeAll(int fd, std::string_view content) {
  const char* data = content.data();
  size_t left = content.size();
  while (left > 0) {
    ssize_t n = ::write(fd, data, left);
    if (n < 0) {
      if (errno == EINTR) {
        continue;
      }
      return false;
    }
    if (n == 0) {
      return false;
    }
    data += n;
    left -= static_cast<size_t>(n);
  }
  return true;
}

// Flushes the folder entry of a rename to disk. Best effort: some file
// systems cannot open or sync a folder.
void syncFolder(const fs::path& folder) {
  int fd = ::open(folder.empty() ? "." : folder.c_str(),
                  O_RDONLY | O_DIRECTORY | O_CLOEXEC);
  if (fd < 0) {
    return;
  }
  ::fsync(fd);
  ::close(fd);
}

}  // namespace

bool writeFileAtomically(const fs::path& path, std::string_view content) {
  const fs::path folder = path.parent_path();
  fs::path tmp = folder / ("." + path.filename().string() + ".tmp-" +
                           std::to_string(static_cast<long>(::getpid())));
  int fd = ::open(tmp.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
  if (fd < 0) {
    return false;
  }
  // On network file systems a write error may only show up at fsync or
  // close; a temporary file that was not fully written must never replace
  // the real one.
  bool ok = writeAll(fd, content);
  if (ok && ::fsync(fd) != 0) {
    ok = false;
  }
  if (::close(fd) != 0) {
    ok = false;
  }
  if (!ok) {
    std::error_code ec;
    fs::remove(tmp, ec);
    return false;
  }
  std::error_code ec;
  fs::rename(tmp, path, ec);
  if (ec) {
    std::error_code removeEc;
    fs::remove(tmp, removeEc);
    return false;
  }
  syncFolder(folder);
  return true;
}

bool createFileIfAbsent(const fs::path& path, std::string_view content) {
  int fd = ::open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0644);
  if (fd < 0) {
    return false;
  }
  bool ok = writeAll(fd, content);
  if (::close(fd) != 0) {
    ok = false;
  }
  return ok;
}

}  // namespace McBopomofo::SmartLearning
