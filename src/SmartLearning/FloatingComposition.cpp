#include "FloatingComposition.h"

#include <algorithm>
#include <utility>

namespace McBopomofo::SmartLearning {

bool UsesFloatingComposition(const std::string& program,
                             const std::vector<std::string>& programs) {
  if (program.empty()) {
    return false;
  }
  return std::find(programs.begin(), programs.end(), program) !=
         programs.end();
}

bool UseClientPreedit(bool clientSupportsPreedit, const std::string& program,
                      const std::vector<std::string>& programs) {
  return clientSupportsPreedit && !UsesFloatingComposition(program, programs);
}

FloatingCompositionToggle ToggleFloatingComposition(
    std::vector<std::string> programs, const std::string& program) {
  if (program.empty()) {
    return {std::move(programs), false};
  }
  if (UsesFloatingComposition(program, programs)) {
    std::erase(programs, program);
    return {std::move(programs), false};
  }
  programs.push_back(program);
  return {std::move(programs), true};
}

}  // namespace McBopomofo::SmartLearning
