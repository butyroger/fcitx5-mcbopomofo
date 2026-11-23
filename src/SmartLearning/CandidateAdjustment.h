#ifndef SRC_SMARTLEARNING_CANDIDATEADJUSTMENT_H_
#define SRC_SMARTLEARNING_CANDIDATEADJUSTMENT_H_

#include <cstddef>
#include <string>

// Rules for raising ("+") or excluding ("-") the highlighted candidate of the
// candidate list. Both work on a phrase of one or more characters.
namespace McBopomofo::SmartLearning {

enum class AdjustCheck {
  // The candidate can be raised or excluded; ask for confirmation.
  kAllowed,
  // Not adjustable, and the key is swallowed without a sound. This is the
  // case of a candidate whose value is an expanded input macro.
  kIgnoreSilently,
  // Not adjustable: report an error (a beep) and swallow the key. This is
  // the case of punctuation, symbols, literal text, English words and
  // anything else whose reading starts with an underscore, and of a value
  // that is empty or has white space or a line break.
  kRejectWithError,
};

// |reading| and |value| are those of the candidate as listed, |rawValue| is
// its value before any input macro was expanded. A single character, such as
// one with the reading "ㄋㄧˇ", is adjustable like a longer phrase.
AdjustCheck CheckCandidateAdjustable(const std::string& reading,
                                     const std::string& value,
                                     const std::string& rawValue);

// The page of the candidate list that shows the candidate at the (global)
// index, for a list showing |pageSize| candidates a page. Used to bring the
// highlight back to where it was. A |pageSize| of 0 gives page 0.
size_t PageOfCandidateIndex(size_t globalIndex, size_t pageSize);

}  // namespace McBopomofo::SmartLearning

#endif  // SRC_SMARTLEARNING_CANDIDATEADJUSTMENT_H_
