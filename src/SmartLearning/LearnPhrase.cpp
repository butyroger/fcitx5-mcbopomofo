#include "LearnPhrase.h"

#include "UserPhraseFile.h"

namespace McBopomofo::SmartLearning {

using Formosa::Gramambular2::ReadingGrid;

namespace {

bool isExpandedMacro(const Formosa::Gramambular2::LanguageModel::Unigram& u) {
  return !u.rawValue().empty() && u.rawValue() != u.value();
}

LearnAction actionFor(std::string value, std::string reading,
                      size_t observations) {
  LearnAction action;
  action.kind = observations < kLearnPhraseThreshold
                    ? LearnAction::kAddTemporary
                    : LearnAction::kLearnUserPhrase;
  action.value = std::move(value);
  action.reading = std::move(reading);
  return action;
}

}  // namespace

LearnAction PlanLearnPickedPhrase(const ReadingGrid::NodePtr& node,
                                  const UserOverrideModel& model) {
  if (node == nullptr || node->spanningLength() < 2) {
    return {};
  }
  const auto unigram = node->currentUnigram();
  if (isExpandedMacro(unigram)) {
    return {};
  }
  const std::string& value = unigram.value();
  const std::string& reading = node->reading();
  if (!UserPhraseFile::fitsInColumn(value)) {
    return {};
  }
  if (reading.empty() || reading[0] == '_' ||
      reading.find('-') == std::string::npos) {
    return {};
  }
  return actionFor(value, reading, model.candidateObservationCount(value));
}

LearnAction PlanLearnBrokenUpPhrase(const ReadingGrid::NodePtr& node,
                                    const ReadingGrid::WalkResult& before,
                                    const ReadingGrid::WalkResult& after,
                                    size_t cursor,
                                    const UserOverrideModel& model) {
  if (node == nullptr || node->spanningLength() != 1) {
    return {};
  }
  size_t phraseEnd = 0;
  auto oldNode = before.findNodeAt(cursor, &phraseEnd);
  if (oldNode == before.nodes.cend()) {
    return {};
  }
  size_t phraseLength = (*oldNode)->spanningLength();
  if (phraseLength < 2 || phraseLength > 4) {
    return {};
  }
  size_t phraseStart = phraseEnd - phraseLength;

  // The nodes that now cover the span of the old phrase.
  std::string value;
  std::string reading;
  size_t position = 0;
  for (const auto& n : after.nodes) {
    size_t start = position;
    position += n->spanningLength();
    if (position <= phraseStart) {
      continue;
    }
    if (start >= phraseEnd) {
      break;
    }
    if (start < phraseStart || position > phraseEnd) {
      return {};
    }
    const auto unigram = n->currentUnigram();
    if (n->reading().empty() || n->reading()[0] == '_' ||
        isExpandedMacro(unigram)) {
      return {};
    }
    value += unigram.value();
    if (!reading.empty()) {
      reading += '-';
    }
    reading += n->reading();
  }
  if (!UserPhraseFile::fitsInColumn(value)) {
    return {};
  }
  return actionFor(
      std::move(value), std::move(reading),
      model.observationCount(after, cursor, node->currentUnigram().value()));
}

std::vector<Phrase> PhrasesToLearnOnCommit(
    const ReadingGrid::WalkResult& walk,
    const std::set<Phrase>& pickedInComposition,
    const std::function<bool(const std::string&, const std::string&)>&
        isTemporary) {
  std::vector<Phrase> phrases;
  std::set<Phrase> listed;
  for (const auto& node : walk.nodes) {
    const std::string& reading = node->reading();
    if (node->spanningLength() < 2 || reading.empty() || reading[0] == '_') {
      continue;
    }
    Phrase phrase(node->currentUnigram().value(), reading);
    if (pickedInComposition.count(phrase) > 0 || listed.count(phrase) > 0 ||
        !isTemporary(phrase.first, phrase.second)) {
      continue;
    }
    listed.insert(phrase);
    phrases.push_back(std::move(phrase));
  }
  return phrases;
}

}  // namespace McBopomofo::SmartLearning
