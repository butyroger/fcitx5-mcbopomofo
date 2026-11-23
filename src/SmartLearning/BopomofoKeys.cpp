#include "BopomofoKeys.h"

#include <algorithm>
#include <cctype>

namespace McBopomofo::SmartLearning {

using Formosa::Mandarin::BopomofoReadingBuffer;
using Formosa::Mandarin::BopomofoSyllable;

size_t ComponentCount(const BopomofoSyllable& syllable) {
  return (syllable.hasConsonant() ? 1 : 0) +
         (syllable.hasMiddleVowel() ? 1 : 0) + (syllable.hasVowel() ? 1 : 0);
}

bool KeyReplacesComponent(const BopomofoReadingBuffer& buffer, char key) {
  if (!buffer.isValidKey(key)) {
    return false;
  }
  BopomofoReadingBuffer copy(buffer);
  copy.combineKey(key);
  return !copy.hasToneMarker() &&
         ComponentCount(copy.syllable()) <= ComponentCount(buffer.syllable());
}

std::string RawKeysOfCurrentReading(const BopomofoReadingBuffer& buffer,
                                    const std::string& rawKeys) {
  if (buffer.isEmpty()) {
    return "";
  }
  BopomofoReadingBuffer replay(buffer.keyboardLayout());
  for (char k : rawKeys) {
    replay.combineKey(k);
  }
  if (replay.composedString() == buffer.composedString()) {
    return rawKeys;
  }
  return buffer.keyboardLayout()->keySequenceFromSyllable(buffer.syllable());
}

std::optional<TrailingConversion> PlanTrailingEnglishToChinese(
    const std::string& run, char key,
    const Formosa::Mandarin::BopomofoKeyboardLayout* layout,
    const std::function<bool(const std::string&)>& hasUnigrams,
    const std::function<bool(const std::string&)>& isWord,
    const std::function<bool(const std::string&)>& isWordOrPrefix) {
  if (static_cast<unsigned char>(key) >= 0x80) {
    return std::nullopt;
  }
  bool isSpace = key == ' ';
  // With a letter key, the whole letters may still be an English word being
  // typed, like "helpf" for helpful.
  if (isSpace) {
    if (isWord(run)) {
      return std::nullopt;
    }
  } else if (isalpha(static_cast<unsigned char>(key)) &&
             isWordOrPrefix(run + key)) {
    return std::nullopt;
  }

  // A syllable has 3 keys at most, plus the tone key.
  for (size_t count = std::min<size_t>(3, run.size()); count >= 1; count--) {
    std::string prefix = run.substr(0, run.size() - count);
    if (prefix.size() < 2) {
      continue;
    }
    std::string keys = run.substr(run.size() - count);
    BopomofoReadingBuffer buffer(layout);
    bool valid = true;
    for (char k : keys) {
      if (!buffer.isValidKey(k)) {
        valid = false;
        break;
      }
      buffer.combineKey(k);
    }
    // The keys have to be exactly how the syllable is typed, without a tone.
    if (!valid || buffer.isEmpty() || buffer.hasToneMarker() ||
        layout->keySequenceFromSyllable(buffer.syllable()) != keys) {
      continue;
    }
    if (!isSpace) {
      if (!buffer.isValidKey(key)) {
        continue;
      }
      buffer.combineKey(key);
      if (!buffer.hasToneMarker() || buffer.hasToneMarkerOnly()) {
        continue;
      }
    }
    std::string reading = buffer.syllable().composedString();
    if (!hasUnigrams(reading)) {
      continue;
    }
    if (!isWord(prefix)) {
      continue;
    }
    return TrailingConversion{count, reading};
  }
  return std::nullopt;
}

}  // namespace McBopomofo::SmartLearning
