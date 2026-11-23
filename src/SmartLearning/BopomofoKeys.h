#ifndef SRC_SMARTLEARNING_BOPOMOFOKEYS_H_
#define SRC_SMARTLEARNING_BOPOMOFOKEYS_H_

#include <cstddef>
#include <functional>
#include <optional>
#include <string>

#include "Mandarin/Mandarin.h"

namespace McBopomofo::SmartLearning {

// How many of the consonant, the middle vowel and the vowel a syllable has.
size_t ComponentCount(const Formosa::Mandarin::BopomofoSyllable& syllable);

// Whether |key| only replaces a part of the unfinished reading (like the second
// "p" in "app", which would make a syllable no longer than the current one and
// without a tone), so it does not make Chinese.
bool KeyReplacesComponent(const Formosa::Mandarin::BopomofoReadingBuffer& buffer,
                          char key);

// The keys typed for the current reading. If |rawKeys| is out of sync with the
// reading, such as after Backspace, gives the keys that make the reading.
std::string RawKeysOfCurrentReading(
    const Formosa::Mandarin::BopomofoReadingBuffer& buffer,
    const std::string& rawKeys);

// The result of turning the letters before the cursor into Chinese.
struct TrailingConversion {
  // How many of the trailing letters are replaced.
  size_t lettersToDelete;
  // The reading (Bopomofo with the tone) to put in their place.
  std::string reading;
};

// Plans turning the last letters of |run| (the letters right before the
// cursor) into a Bopomofo syllable when |key| (a tone key, or a space for the
// first tone) is typed. Nothing is planned if the letters may still be an
// English word, or if what remains is not an English word of 2 letters or more.
std::optional<TrailingConversion> PlanTrailingEnglishToChinese(
    const std::string& run, char key,
    const Formosa::Mandarin::BopomofoKeyboardLayout* layout,
    const std::function<bool(const std::string&)>& hasUnigrams,
    const std::function<bool(const std::string&)>& isWord,
    const std::function<bool(const std::string&)>& isWordOrPrefix);

}  // namespace McBopomofo::SmartLearning

#endif  // SRC_SMARTLEARNING_BOPOMOFOKEYS_H_
