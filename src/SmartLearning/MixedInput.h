#ifndef SRC_SMARTLEARNING_MIXEDINPUT_H_
#define SRC_SMARTLEARNING_MIXEDINPUT_H_

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace McBopomofo::SmartLearning {

// The reading of a space put between Chinese and English.
inline constexpr char kAutoSpaceReading[] = "_literal_ ";

// A letter typed in the smart English mode, or an English word picked from the
// completions ("_english_...").
bool IsEnglishReading(const std::string& reading);

// An English letter or word, or a half-width digit.
bool IsHalfWidthAlphanumericReading(const std::string& reading);

// A Bopomofo syllable, or a Chinese character put as is. Punctuation, digits
// and other special readings start with "_".
bool IsChineseReading(const std::string& reading);

// "_literal_" followed by one ASCII letter.
bool IsLetterReading(const std::string& reading);

// The letters in the run of letter readings right before |cursor|. English
// words picked from the completions are not included.
std::string TrailingLetters(const std::vector<std::string>& readings,
                            size_t cursor);

// Replacing the letters right before the cursor with one English word reading,
// whose candidates are the word and its completions.
struct EnglishCompletionPlan {
  // The letter readings to delete going backwards from the cursor.
  size_t lettersToDelete;
  // "_english_" followed by the letters.
  std::string englishReading;
};

// The plan for listing the completions of the letters before |cursor|, or
// nothing if there are fewer than two letters. Whether there are any
// completions is checked by the caller.
std::optional<EnglishCompletionPlan> PlanEnglishCompletion(
    const std::vector<std::string>& readings, size_t cursor);

// Whether a space is needed at |cursor|: after Chinese when English follows
// (|beforeEnglish|), or after English when Chinese follows. The preference and
// the input mode are checked by the caller.
bool AutoSpaceNeeded(const std::vector<std::string>& readings, size_t cursor,
                     bool beforeEnglish);

// After the last English reading before |cursor| is deleted (|readings| is the
// state after the deletion): whether the space before the cursor should be
// deleted as well.
bool ShouldRemoveAutoSpaceAfterDeletingEnglish(
    const std::vector<std::string>& readings, size_t cursor);

// Plans trimming mistyped text before |cursor|: returns how many readings to
// delete going backwards from the cursor. Stops at Chinese, English words
// picked from the completions and digits; a run of letters keeps its longest
// prefix (of 3 letters or more) that is a word, or all of it if it is a word.
size_t PlanTrimBeforeCursor(
    const std::vector<std::string>& readings, size_t cursor,
    const std::function<bool(const std::string&)>& isWord);

// "英" when Caps Lock is on or the smart English mode is active, else "中".
std::string LanguageModeLabel(bool capsLockOn, bool smartEnglish);

// The reminder to show when the Caps Lock state goes from |previous| to
// |current|: "ABC" when it turns on, "中文" when it turns off, nothing when it
// did not change. While the state is not |known| yet (right after activation),
// the first state is only adopted as the baseline, so nothing is shown.
std::optional<std::string> SwitchReminderText(bool known, bool previous,
                                              bool current);

// "半" for half width, else "全".
std::string WidthModeLabel(bool halfWidth);

}  // namespace McBopomofo::SmartLearning

#endif  // SRC_SMARTLEARNING_MIXEDINPUT_H_
