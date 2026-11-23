#ifndef SRC_SMARTLEARNING_FLOATINGCOMPOSITION_H_
#define SRC_SMARTLEARNING_FLOATINGCOMPOSITION_H_

#include <string>
#include <vector>

// Floating composition for chosen programs. Some programs change the text
// being composed by themselves, which drops or repeats characters. For the
// programs the user chooses, the text being composed is shown by the input
// method instead, and the program only receives the committed text.
//
// Programs are named as the input method front end reports them; an empty
// name never matches.
namespace McBopomofo::SmartLearning {

// Whether |program| is one of |programs|. Names match exactly.
bool UsesFloatingComposition(const std::string& program,
                             const std::vector<std::string>& programs);

// Whether to put the text being composed into the program: only when the
// program can show it (|clientSupportsPreedit|) and does not use floating
// composition.
bool UseClientPreedit(bool clientSupportsPreedit, const std::string& program,
                      const std::vector<std::string>& programs);

struct FloatingCompositionToggle {
  std::vector<std::string> programs;
  // Whether |program| uses floating composition after the toggle.
  bool enabled = false;
};

// Adds |program| to the end of |programs|, or removes every copy of it,
// keeping the order of the others. An empty program changes nothing.
FloatingCompositionToggle ToggleFloatingComposition(
    std::vector<std::string> programs, const std::string& program);

}  // namespace McBopomofo::SmartLearning

#endif  // SRC_SMARTLEARNING_FLOATINGCOMPOSITION_H_
