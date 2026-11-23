#include "LearnedVocabulary.h"

#include <chrono>
#include <cmath>
#include <ctime>
#include <utility>

#include "CommonSentences.h"
#include "EnglishWords.h"
#include "RemovedEntries.h"
#include "TemporaryPhrases.h"
#include "UserPhraseFile.h"

namespace McBopomofo::VocabularyManager {

namespace fs = std::filesystem;
using SmartLearning::CommonSentenceStore;
using SmartLearning::EnglishWordStore;
using SmartLearning::RemovedEntries;
using SmartLearning::TemporaryPhraseStore;

namespace {

double steadySeconds() {
  return std::chrono::duration<double>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

double systemSeconds() {
  return std::chrono::duration<double>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

// "<value> <reading>" split at the first space.
std::pair<std::string, std::string> splitPhraseKey(const std::string& key) {
  size_t space = key.find(' ');
  if (space == std::string::npos) {
    return {key, ""};
  }
  return {key.substr(0, space), key.substr(space + 1)};
}

bool dataFileHolds(const fs::path& data, const std::string& value,
                   const std::string& reading) {
  for (const auto& row : loadRows(FileKind::kPhrases, data)) {
    if (row.columns.size() == 2 && row.columns[0] == value &&
        row.columns[1] == reading) {
      return true;
    }
  }
  return false;
}

CommitStatus commitFailure(bool pending) {
  return pending ? CommitStatus::kFileMissing : CommitStatus::kWriteFailed;
}

}  // namespace

bool isAddableEnglishWord(std::string_view word) {
  if (word.size() < 2) {
    return false;
  }
  for (char c : word) {
    auto byte = static_cast<unsigned char>(c);
    bool letter = (byte >= 'a' && byte <= 'z') || (byte >= 'A' && byte <= 'Z');
    if (!letter) {
      return false;
    }
  }
  return true;
}

std::string localDate(double unixTime) {
  auto time = static_cast<std::time_t>(std::floor(unixTime));
  std::tm local{};
  if (localtime_r(&time, &local) == nullptr) {
    return {};
  }
  char text[32] = {};
  std::strftime(text, sizeof(text), "%Y-%m-%d", &local);
  return text;
}

LearnedVocabulary::LearnedVocabulary(const fs::path& dataFolder,
                                     const std::string& deviceID,
                                     const std::string& hostName,
                                     std::function<double()> wallNow)
    : dataFolder_(dataFolder),
      learningFolder_(dataFolder / "learning"),
      readOnly_(deviceID.empty()),
      wallNow_(wallNow ? std::move(wallNow) : systemSeconds),
      removedEntries_(std::make_shared<RemovedEntries>(steadySeconds)) {
  // One reader of removed.txt for the three stores, as in the input method.
  englishWords_ = std::make_unique<EnglishWordStore>(
      deviceID, hostName, nullptr, steadySeconds, removedEntries_, wallNow_);
  sentences_ = std::make_unique<CommonSentenceStore>(deviceID, hostName,
                                                     wallNow_, removedEntries_);
  temporaryPhrases_ = std::make_unique<TemporaryPhraseStore>(
      deviceID, hostName, wallNow_, removedEntries_);
  removedEntries_->setLearningFolder(learningFolder_);
  englishWords_->setLearningFolder(learningFolder_);
  sentences_->setLearningFolder(learningFolder_);
  temporaryPhrases_->setLearningFolder(learningFolder_);
}

LearnedVocabulary::~LearnedVocabulary() = default;

DeleteStatus LearnedVocabulary::failure(bool pending) {
  return pending ? DeleteStatus::kFileMissing : DeleteStatus::kWriteFailed;
}

std::vector<VocabularyRow> LearnedVocabulary::rows(
    LearnedKind kind, const SentenceStatusTexts& texts) {
  // Another program may have changed removed.txt since it was last read.
  removedEntries_->reloadIfChanged();
  std::vector<VocabularyRow> rows;
  switch (kind) {
    case LearnedKind::kEnglish:
      for (auto& word : englishWords_->allWords()) {
        rows.push_back(
            {word.word, {word.word, std::to_string(word.count)}, word.word});
      }
      break;
    case LearnedKind::kSentences:
      for (auto& item : sentences_->allEntries()) {
        const auto& entry = item.entry;
        std::string status;
        if (entry.pinned) {
          status = texts.added;
        } else if (entry.count >= CommonSentenceStore::kThreshold) {
          status = texts.remembered;
        } else if (texts.moreTimes) {
          status = texts.moreTimes(CommonSentenceStore::kThreshold - entry.count);
        }
        rows.push_back({item.sentence,
                        {item.sentence, std::to_string(entry.count),
                         std::move(status)},
                        item.sentence});
      }
      break;
    case LearnedKind::kTemporary:
      for (auto& phrase : temporaryPhrases_->allPhrases()) {
        std::string key = phrase.value + " " + phrase.reading;
        rows.push_back({key,
                        {phrase.value, phrase.reading, localDate(phrase.time)},
                        key});
      }
      break;
  }
  return rows;
}

CommitResult LearnedVocabulary::add(LearnedKind kind, std::string_view field) {
  std::string value = trimSpaces(field);
  CommitResult result{CommitStatus::kNothingTyped, value};
  if (value.empty() || kind == LearnedKind::kTemporary) {
    return result;
  }
  if (readOnly_) {
    result.status = CommitStatus::kReadOnly;
    return result;
  }
  removedEntries_->reloadIfChanged();
  if (kind == LearnedKind::kEnglish) {
    if (!isAddableEnglishWord(value)) {
      result.status = CommitStatus::kBadWord;
    } else if (!englishWords_->add(value)) {
      result.status = commitFailure(englishWords_->isLocalFilePending());
    } else {
      result.status = CommitStatus::kAdded;
    }
  } else {
    if (!SmartLearning::IsValidSentence(value)) {
      result.status = CommitStatus::kBadSentence;
    } else if (!sentences_->add(value)) {
      result.status = commitFailure(sentences_->isLocalFilePending());
    } else {
      result.status = CommitStatus::kAdded;
    }
  }
  return result;
}

BatchResult LearnedVocabulary::remove(LearnedKind kind,
                                      const std::vector<std::string>& keys) {
  BatchResult result{DeleteStatus::kNothingSelected, 0};
  if (keys.empty()) {
    return result;
  }
  if (readOnly_) {
    result.status = DeleteStatus::kReadOnly;
    return result;
  }
  removedEntries_->reloadIfChanged();
  for (const auto& key : keys) {
    bool removed = false;
    bool pending = false;
    switch (kind) {
      case LearnedKind::kEnglish:
        removed = englishWords_->remove(key);
        pending = englishWords_->isLocalFilePending();
        break;
      case LearnedKind::kSentences:
        removed = sentences_->remove(key);
        pending = sentences_->isLocalFilePending();
        break;
      case LearnedKind::kTemporary: {
        auto [value, reading] = splitPhraseKey(key);
        removed = temporaryPhrases_->forget(value, reading);
        pending = temporaryPhrases_->isLocalFilePending();
        break;
      }
    }
    if (!removed) {
      result.status = failure(pending);
      return result;
    }
    ++result.done;
  }
  result.status = DeleteStatus::kDeleted;
  return result;
}

BatchResult LearnedVocabulary::addToPhrases(
    const std::vector<std::string>& keys) {
  BatchResult result{DeleteStatus::kNothingSelected, 0};
  if (keys.empty()) {
    return result;
  }
  if (readOnly_) {
    result.status = DeleteStatus::kReadOnly;
    return result;
  }
  fs::path data = vocabularyFile(dataFolder_, FileKind::kPhrases);
  fs::path exclude = vocabularyFile(dataFolder_, FileKind::kExcluded);
  std::error_code ec;
  if (!fs::exists(data, ec)) {
    // data.txt is not created: the folder may still be syncing.
    result.status = DeleteStatus::kFileMissing;
    return result;
  }
  removedEntries_->reloadIfChanged();
  for (const auto& key : keys) {
    auto [value, reading] = splitPhraseKey(key);
    // Gives false for a phrase already in data.txt too; it is then only
    // forgotten.
    if (!SmartLearning::UserPhraseFile::write(data, exclude, value, reading) &&
        !dataFileHolds(data, value, reading)) {
      result.status = fs::exists(data, ec) ? DeleteStatus::kWriteFailed
                                           : DeleteStatus::kFileMissing;
      return result;
    }
    // Counted once data.txt has the phrase, so that the input method is told
    // even if forgetting it fails.
    ++result.done;
    if (!temporaryPhrases_->forget(value, reading)) {
      result.status = failure(temporaryPhrases_->isLocalFilePending());
      return result;
    }
  }
  result.status = DeleteStatus::kDeleted;
  return result;
}

}  // namespace McBopomofo::VocabularyManager
