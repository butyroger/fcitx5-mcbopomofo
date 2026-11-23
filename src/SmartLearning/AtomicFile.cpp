#include "AtomicFile.h"

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

}  // namespace McBopomofo::SmartLearning
