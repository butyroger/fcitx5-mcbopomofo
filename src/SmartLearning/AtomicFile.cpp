#include "AtomicFile.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <fstream>
#include <sstream>
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
  // A new file gets 0666 less the umask, which open applies by itself; this
  // reads no process-wide state, so it is safe with other threads.
  int fd = ::open(tmp.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0666);
  if (fd < 0) {
    return false;
  }
  // The new content replaces the file, so it keeps the file's permissions:
  // a shared file must not become readable only by its owner.
  struct stat existing {};
  if (::stat(path.c_str(), &existing) == 0 && S_ISREG(existing.st_mode) &&
      ::fchmod(fd, existing.st_mode & 07777) != 0) {
    ::close(fd);
    std::error_code ec;
    fs::remove(tmp, ec);
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

std::optional<std::string> readWholeFile(const fs::path& path) {
  std::error_code ec;
  if (!fs::is_regular_file(path, ec)) {
    return std::nullopt;
  }
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    return std::nullopt;
  }
  std::ostringstream ss;
  ss << in.rdbuf();
  if (in.bad()) {
    return std::nullopt;
  }
  return ss.str();
}

FileStamp stampOf(const fs::path& path) {
  FileStamp stamp;
  std::error_code ec;
  if (path.empty() || !fs::is_regular_file(path, ec)) {
    return stamp;
  }
  stamp.exists = true;
  stamp.modified = fs::last_write_time(path, ec);
  stamp.size = fs::file_size(path, ec);
  if (ec) {
    stamp.size = 0;
  }
  return stamp;
}

std::vector<std::string_view> splitLines(std::string_view text) {
  std::vector<std::string_view> lines;
  size_t start = 0;
  size_t i = 0;
  while (i < text.size()) {
    size_t breakLength = 0;
    auto c = static_cast<unsigned char>(text[i]);
    if (c >= 0x0A && c <= 0x0D) {
      breakLength = 1;
    } else if (c == 0xC2 && i + 1 < text.size() &&
               static_cast<unsigned char>(text[i + 1]) == 0x85) {
      breakLength = 2;
    } else if (c == 0xE2 && i + 2 < text.size() &&
               static_cast<unsigned char>(text[i + 1]) == 0x80 &&
               (static_cast<unsigned char>(text[i + 2]) == 0xA8 ||
                static_cast<unsigned char>(text[i + 2]) == 0xA9)) {
      breakLength = 3;
    }
    if (breakLength == 0) {
      ++i;
      continue;
    }
    lines.push_back(text.substr(start, i - start));
    i += breakLength;
    start = i;
  }
  lines.push_back(text.substr(start));
  return lines;
}

}  // namespace McBopomofo::SmartLearning
