#include "Translation.h"

#include <libintl.h>

#include <string>

namespace McBopomofo::VocabularyManager {

void initTranslation() {
  bindtextdomain(MCBOPOMOFO_GETTEXT_DOMAIN, MCBOPOMOFO_LOCALEDIR);
  bind_textdomain_codeset(MCBOPOMOFO_GETTEXT_DOMAIN, "UTF-8");
}

QString translate(const char* msgid) {
  return QString::fromUtf8(dgettext(MCBOPOMOFO_GETTEXT_DOMAIN, msgid));
}

QString translate(const char* context, const char* msgid) {
  // gettext keeps a msgctxt as "<context>\004<msgid>".
  std::string key = std::string(context) + '\004' + msgid;
  const char* translated = dgettext(MCBOPOMOFO_GETTEXT_DOMAIN, key.c_str());
  if (translated == key.c_str() || key == translated) {
    return QString::fromUtf8(msgid);
  }
  return QString::fromUtf8(translated);
}

}  // namespace McBopomofo::VocabularyManager
