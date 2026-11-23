#include "AtomicFile.h"

#include <fcntl.h>
#include <unistd.h>

#include <fstream>
#include <string>
#include <system_error>

namespace McBopomofo::SmartLearning {
namespace fs = std::filesystem;

bool writeFileAtomically(const fs::path& path, std::string_view content) {
  fs::path tmp = path.parent_path() /
                 ("." + path.filename().string() + ".tmp-" +
                  std::to_string(static_cast<long>(::getpid())));
  {
    std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
    if (!out) {
      return false;
    }
    out.write(content.data(), static_cast<std::streamsize>(content.size()));
    out.flush();
    if (!out) {
      out.close();
      std::error_code ec;
      fs::remove(tmp, ec);
      return false;
    }
  }
  std::error_code ec;
  fs::rename(tmp, path, ec);
  if (ec) {
    std::error_code removeEc;
    fs::remove(tmp, removeEc);
    return false;
  }
  return true;
}

bool createFileIfAbsent(const fs::path& path, std::string_view content) {
  int fd = ::open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0644);
  if (fd < 0) {
    return false;
  }
  bool ok = true;
  const char* data = content.data();
  size_t left = content.size();
  while (left > 0) {
    ssize_t n = ::write(fd, data, left);
    if (n < 0) {
      ok = false;
      break;
    }
    data += n;
    left -= static_cast<size_t>(n);
  }
  if (::close(fd) != 0) {
    ok = false;
  }
  return ok;
}

}  // namespace McBopomofo::SmartLearning
