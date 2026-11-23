#ifndef SRC_VOCABULARYMANAGER_MAINWINDOW_H_
#define SRC_VOCABULARYMANAGER_MAINWINDOW_H_

#include <QString>
#include <QStringList>
#include <QWidget>
#include <filesystem>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "FileVocabulary.h"
#include "LearnedVocabulary.h"
#include "ManagerSettings.h"

class QLabel;
class QLineEdit;
class QPushButton;
class QTabBar;
class QTreeWidget;

namespace McBopomofo::VocabularyManager {

// The window listing the user's vocabulary, one tab for each kind, where
// entries are added, edited and deleted.
class MainWindow : public QWidget {
  Q_OBJECT

 public:
  MainWindow(Settings settings, std::filesystem::path builtInData);
  ~MainWindow() override;

  // Shows the window in front, with the lists read again.
  void bringToFront();

  // Uses the folder and the device ID of |settings| from now on, as when a
  // read-only window is opened again by the input method.
  void adoptSettings(Settings settings);
  [[nodiscard]] const Settings& currentSettings() const { return settings_; }

  // Loads the built-in phrase data used to fill in the readings.
  void preloadReadings();

  // Reads the current tab's file again; the rows still there stay selected,
  // and the list stays scrolled where it was.
  void reload();

  int pageCount() const;
  QString pageTitle(int index) const;
  void selectPage(int index);
  // The rows of the current tab, before the search.
  size_t rowCount() const { return rows_.size(); }

 protected:
  bool event(QEvent* event) override;

 private:
  enum class PageKind {
    kPhrases,
    kExcluded,
    kEnglish,
    kSentences,
    kTemporary,
    kTemplates
  };

  struct Page {
    PageKind kind;
    QString title;
    QString help;
    QStringList columns;
    QStringList fields;
    // The reading is filled in from the phrase.
    bool fillsReading;
    // An entry can be edited.
    bool canEdit;
    // The second column (the reading, the text) stretches with the first.
    bool wideSecondColumn;
  };

  const Page& page() const;
  // The file of the tab, for the tabs that list the lines of a file.
  std::optional<FileKind> pageFileKind() const;
  // The learned entries of the tab, for the other tabs.
  std::optional<LearnedKind> pageLearnedKind() const;
  std::filesystem::path pageFile() const;
  SentenceStatusTexts sentenceStatusTexts() const;

  void pageChanged(int index);
  void loadRows();
  void showRows();
  void phraseEdited();
  void readingEdited();
  void beginEditing();
  void cancelEditing();
  void commit();
  void deleteSelected();
  void addSelectedToPhrases();
  void revealFile();
  void updateButtons();
  void updateNotice();
  // Shows the message of a status other than success; false on success.
  bool showFailure(CommitStatus status, const std::string& value);
  bool showFailure(DeleteStatus status);
  std::vector<const VocabularyRow*> selectedRows() const;
  void notifyInputMethod();

  Settings settings_;
  ReadingFiller readingFiller_;
  std::unique_ptr<LearnedVocabulary> learned_;
  std::vector<Page> pages_;
  int pageIndex_ = 0;
  std::vector<VocabularyRow> rows_;
  std::set<std::string> selection_;
  std::optional<std::string> editingKey_;
  // The user typed the reading, so it is not filled in.
  bool readingTyped_ = false;

  QTabBar* tabs_;
  QLabel* help_;
  QLabel* notice_;
  QLineEdit* search_;
  QLabel* count_;
  QTreeWidget* list_;
  QWidget* editor_;
  QLineEdit* field1_;
  QLineEdit* field2_;
  QPushButton* addButton_;
  QPushButton* cancelButton_;
  QPushButton* editButton_;
  QPushButton* deleteButton_;
  QPushButton* addToPhrasesButton_;
  QLabel* message_;
  QPushButton* revealButton_;
};

}  // namespace McBopomofo::VocabularyManager

#endif  // SRC_VOCABULARYMANAGER_MAINWINDOW_H_
