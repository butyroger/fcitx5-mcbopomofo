#ifndef SRC_SMARTLEARNING_READINGEXTRACTION_H_
#define SRC_SMARTLEARNING_READINGEXTRACTION_H_

#include <optional>
#include <string>

#include "McBopomofoLM.h"

namespace McBopomofo::SmartLearning {

// The reading of |phrase| made of the readings of the longest pieces that the
// built-in language model knows, looked at from the start of the phrase. The
// readings are joined with "-". A character that no piece covers gives "？"
// and the next piece starts after it. For a piece with several readings, the
// reading with the highest score is used. The user's own phrases are not
// looked at, and no conversion between simplified and traditional characters
// is done.
//
// Returns nullopt if |phrase| is empty, or nothing in it is found.
std::optional<std::string> ExtractReading(const McBopomofoLM& lm,
                                          const std::string& phrase);

}  // namespace McBopomofo::SmartLearning

#endif  // SRC_SMARTLEARNING_READINGEXTRACTION_H_
