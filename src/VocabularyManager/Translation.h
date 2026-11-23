#ifndef SRC_VOCABULARYMANAGER_TRANSLATION_H_
#define SRC_VOCABULARYMANAGER_TRANSLATION_H_

#include <QString>

// The vocabulary manager is translated with the input method's gettext
// domain, so that both share po/zh_TW.po.
namespace McBopomofo::VocabularyManager {

// Binds the gettext domain to the installed translations.
void initTranslation();

QString translate(const char* msgid);

// A translation in |context|, for a text the input method translates
// differently.
QString translate(const char* context, const char* msgid);

}  // namespace McBopomofo::VocabularyManager

#define _(x) ::McBopomofo::VocabularyManager::translate(x)
#define C_(c, x) ::McBopomofo::VocabularyManager::translate(c, x)

#endif  // SRC_VOCABULARYMANAGER_TRANSLATION_H_
