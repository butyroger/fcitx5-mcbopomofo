#include "ReadingExtraction.h"

#include <vector>

namespace McBopomofo::SmartLearning {

namespace {

// The byte offsets where each UTF-8 character starts, and the end.
std::vector<size_t> characterOffsets(const std::string& s) {
  std::vector<size_t> offsets;
  for (size_t i = 0; i < s.size(); ++i) {
    if ((static_cast<unsigned char>(s[i]) & 0xC0) != 0x80) {
      offsets.push_back(i);
    }
  }
  offsets.push_back(s.size());
  return offsets;
}

}  // namespace

std::optional<std::string> ExtractReading(const McBopomofoLM& lm,
                                          const std::string& phrase) {
  std::vector<size_t> offsets = characterOffsets(phrase);
  size_t count = offsets.size() - 1;
  if (count == 0) {
    return std::nullopt;
  }

  std::string result;
  bool found = false;
  size_t from = 0;
  while (from < count) {
    size_t matched = 0;
    for (size_t to = count; to > from; --to) {
      std::string candidate =
          phrase.substr(offsets[from], offsets[to] - offsets[from]);
      std::string reading = lm.getReading(candidate);
      if (!reading.empty()) {
        if (!result.empty()) {
          result += '-';
        }
        result += reading;
        found = true;
        matched = to - from;
        break;
      }
    }
    if (matched == 0) {
      if (!result.empty()) {
        result += '-';
      }
      result += "？";
      matched = 1;
    }
    from += matched;
  }
  if (!found) {
    return std::nullopt;
  }
  return result;
}

}  // namespace McBopomofo::SmartLearning
