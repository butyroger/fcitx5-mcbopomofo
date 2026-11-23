#ifndef SRC_SMARTLEARNING_LEARNPHRASE_H_
#define SRC_SMARTLEARNING_LEARNPHRASE_H_

#include <cstddef>
#include <functional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "UserOverrideModel.h"
#include "gramambular2/reading_grid.h"

// Learning phrases in two steps. A phrase the user picks becomes a temporary
// phrase first; picked again (here or on another device), or typed again and
// committed in a later composition, it is learned as a user phrase.
//
// The functions here only decide what to learn; the caller adds or removes
// the temporary phrases and writes the user phrase file.
namespace McBopomofo::SmartLearning {

// A picked phrase is learned as a user phrase once the user has picked it
// this many times, counting the pick being handled.
inline constexpr size_t kLearnPhraseThreshold = 2;

// The value and the reading of a phrase, the syllables of the reading joined
// by "-".
using Phrase = std::pair<std::string, std::string>;

struct LearnAction {
  enum Kind { kNone, kAddTemporary, kLearnUserPhrase };
  Kind kind = kNone;
  std::string value;
  std::string reading;
};

// What to learn when the user has picked the candidate of |node|, a phrase of
// two characters or more, with |model| already holding the pick: the phrase
// becomes a temporary phrase, or a user phrase once its value has been picked
// kLearnPhraseThreshold times in any context. Expanded input macros, values
// holding white space and readings with a prefix such as "_literal_" are not
// learned.
//
// A phrase of four characters or more is never observed by the model, so
// picking it only makes it a temporary phrase again; it is learned when it is
// typed again and committed (PhrasesToLearnOnCommit).
LearnAction PlanLearnPickedPhrase(
    const Formosa::Gramambular2::ReadingGrid::NodePtr& node,
    const UserOverrideModel& model);

// What to learn when the user has picked a single character, |node|, that
// broke up the phrase of two to four characters at |cursor| in |before|: the
// characters that now cover the span of that phrase in |after| (such as 許氏
// after the user picked 氏 in 許是) become a temporary phrase, or a user phrase
// once the character has been picked kLearnPhraseThreshold times in the same
// context. Without that, the next walk would give the old phrase again.
LearnAction PlanLearnBrokenUpPhrase(
    const Formosa::Gramambular2::ReadingGrid::NodePtr& node,
    const Formosa::Gramambular2::ReadingGrid::WalkResult& before,
    const Formosa::Gramambular2::ReadingGrid::WalkResult& after,
    size_t cursor, const UserOverrideModel& model);

// The temporary phrases of the composition being committed that were typed
// again, and not just picked in this composition (|pickedInComposition|),
// each listed once, in the order they appear.
std::vector<Phrase> PhrasesToLearnOnCommit(
    const Formosa::Gramambular2::ReadingGrid::WalkResult& walk,
    const std::set<Phrase>& pickedInComposition,
    const std::function<bool(const std::string& value,
                             const std::string& reading)>& isTemporary);

}  // namespace McBopomofo::SmartLearning

#endif  // SRC_SMARTLEARNING_LEARNPHRASE_H_
