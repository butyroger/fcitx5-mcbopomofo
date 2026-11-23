#include "CandidateAdjustment.h"

namespace McBopomofo::SmartLearning {

namespace {

// White space and line breaks, ASCII and the Unicode ones (UTF-8).
bool hasWhiteSpace(const std::string& s) {
  for (size_t i = 0; i < s.size(); ++i) {
    unsigned char c = static_cast<unsigned char>(s[i]);
    if (c == ' ' || (c >= '\t' && c <= '\r') || c == 0x1c || c == 0x1d ||
        c == 0x1e || c == 0x1f) {
      return true;
    }
    if (c == 0xC2 && i + 1 < s.size()) {
      unsigned char n = static_cast<unsigned char>(s[i + 1]);
      if (n == 0x85 || n == 0xA0) {  // NEL, no-break space
        return true;
      }
    }
    if (c == 0xE1 && i + 2 < s.size() &&
        static_cast<unsigned char>(s[i + 1]) == 0x9A &&
        static_cast<unsigned char>(s[i + 2]) == 0x80) {  // U+1680
      return true;
    }
    if (c == 0xE2 && i + 2 < s.size()) {
      unsigned char n1 = static_cast<unsigned char>(s[i + 1]);
      unsigned char n2 = static_cast<unsigned char>(s[i + 2]);
      if (n1 == 0x80 &&
          ((n2 >= 0x80 && n2 <= 0x8A) || n2 == 0xA8 || n2 == 0xA9 ||
           n2 == 0xAF)) {  // U+2000-200A, 2028, 2029, 202F
        return true;
      }
      if (n1 == 0x81 && n2 == 0x9F) {  // U+205F
        return true;
      }
    }
    if (c == 0xE3 && i + 2 < s.size() &&
        static_cast<unsigned char>(s[i + 1]) == 0x80 &&
        static_cast<unsigned char>(s[i + 2]) == 0x80) {  // U+3000
      return true;
    }
  }
  return false;
}

}  // namespace

AdjustCheck CheckCandidateAdjustable(const std::string& reading,
                                     const std::string& value,
                                     const std::string& rawValue) {
  // An expanded input macro is left alone, without a sound.
  if (value != rawValue) {
    return AdjustCheck::kIgnoreSilently;
  }
  if (!reading.empty() && reading[0] == '_') {
    return AdjustCheck::kRejectWithError;
  }
  if (value.empty() || hasWhiteSpace(value)) {
    return AdjustCheck::kRejectWithError;
  }
  return AdjustCheck::kAllowed;
}

size_t PageOfCandidateIndex(size_t globalIndex, size_t pageSize) {
  return pageSize == 0 ? 0 : globalIndex / pageSize;
}

}  // namespace McBopomofo::SmartLearning
