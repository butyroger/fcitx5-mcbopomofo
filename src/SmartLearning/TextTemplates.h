#ifndef SRC_SMARTLEARNING_TEXTTEMPLATES_H_
#define SRC_SMARTLEARNING_TEXTTEMPLATES_H_

#include <ctime>
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "AtomicFile.h"

namespace McBopomofo::SmartLearning {

// A text inserted by typing its code after the backtick key, such as "addr"
// for an address. A format of a quick number has no code.
struct TextTemplateEntry {
  std::string code;
  std::string text;

  bool operator==(const TextTemplateEntry& other) const {
    return code == other.code && text == other.text;
  }
};

// The longest code that can still be typed on: one more character is taken
// after this length, and then the keys are refused.
constexpr size_t kMaxTextTemplateCodeLength = 50;

// Each line of text-templates.txt is a code, a tab, and the text. Lines
// starting with "#" are comments. The code is trimmed of spaces; in the text,
// "\n" is a line break, "\t" a tab, and a backslash before any other
// character stands for that character (so "\\" is a backslash). A line
// without a tab, or with an empty code or text, is skipped.
std::vector<TextTemplateEntry> ParseTextTemplates(std::string_view content);

// The templates for the code typed: all of them for no code; otherwise those
// whose code is the code typed (ignoring case), then those whose code starts
// with it, then those whose code or text contains it.
std::vector<TextTemplateEntry> MatchTextTemplates(
    const std::vector<TextTemplateEntry>& templates, std::string_view code);

// The candidates for the code typed: the formats of a number code (a date,
// an amount, an expression; see QuickNumbers.h), which have no code, or the
// templates matching it.
std::vector<TextTemplateEntry> TextTemplateCandidates(
    const std::vector<TextTemplateEntry>& templates, const std::string& code,
    std::time_t now);

// The text shown for a candidate: the code, two spaces, and the text with
// line breaks shown as "⏎", cut to 30 characters and "…". A quick number is
// shown as is.
std::string TextTemplateDisplayText(const TextTemplateEntry& entry);

// Whether the key is an operator of the number code typed, such as "*" for
// 10*15, which is typed with Shift and is then not taken to pick a candidate.
bool IsNumberOperatorKey(const std::string& code, char key);

// The templates of a file, read again once the file changed. Whether it
// changed is checked at most once every kRecheckInterval seconds, so typing
// a code does not look at the disk for every key.
class TextTemplateStore {
 public:
  static constexpr double kRecheckInterval = 2;  // seconds

  // |now| gives the time in seconds, used only to tell when to check the
  // file again.
  explicit TextTemplateStore(std::function<double()> now);

  // Uses the file at |path|. If |sample| is not empty, it is written to the
  // file by createSampleFileIfNeeded() when the file is missing; it is left
  // empty for a folder whose files must not be created, such as one synced
  // from other computers.
  void setFile(std::filesystem::path path, std::string sample);

  // Writes the sample to the file if there is no file yet, so that the user
  // sees how templates work. An existing file is never touched.
  void createSampleFileIfNeeded();

  const std::vector<TextTemplateEntry>& entries();

 private:
  std::function<double()> now_;
  std::filesystem::path path_;
  std::string sample_;
  bool checked_ = false;
  double lastChecked_ = 0;
  FileStamp stamp_;
  std::vector<TextTemplateEntry> entries_;
};

}  // namespace McBopomofo::SmartLearning

#endif  // SRC_SMARTLEARNING_TEXTTEMPLATES_H_
