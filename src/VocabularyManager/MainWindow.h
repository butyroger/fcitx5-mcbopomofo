#ifndef SRC_VOCABULARYMANAGER_MAINWINDOW_H_
#define SRC_VOCABULARYMANAGER_MAINWINDOW_H_

#include <QString>
#include <QStringList>
#include <QWidget>
#include <filesystem>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "FileVocabulary.h"
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

  // Reads the current tab's file again; the rows still there stay selected.
  void reload();

  int pageCount() const;
  QString pageTitle(int index) const;
  void selectPage(int index);
  // The rows of the current tab, before the search.
  size_t rowCount() const { return rows_.size(); }

 protected:
  bool event(QEvent* event) override;

 private:
  struct Page {
    FileKind kind;
    QString title;
    QString help;
    QStringList columns;
    QStringList fields;
    // The reading is filled in from the phrase.
    bool fillsReading;
  };

  const Page& page() const;
  std::filesystem::path pageFile() const;

  void pageChanged(int index);
  void showRows();
  void phraseEdited();
  void readingEdited();
  void beginEditing();
  void cancelEditing();
  void commit();
  void deleteSelected();
  void revealFile();
  void updateButtons();
  void updateNotice();
  std::vector<const VocabularyRow*> selectedRows() const;
  void notifyInputMethod();

  Settings settings_;
  ReadingFiller readingFiller_;
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
  QLabel* message_;
  QPushButton* revealButton_;
};

}  // namespace McBopomofo::VocabularyManager

#endif  // SRC_VOCABULARYMANAGER_MAINWINDOW_H_
