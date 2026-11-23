#include "MainWindow.h"

#include <QApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDesktopServices>
#include <QEvent>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QTabBar>
#include <QTreeWidget>
#include <QUrl>
#include <QVBoxLayout>
#include <algorithm>

#include "Translation.h"

namespace McBopomofo::VocabularyManager {

namespace fs = std::filesystem;

namespace {

QString qstr(const std::string& text) { return QString::fromStdString(text); }

std::string str(const QString& text) { return text.toStdString(); }

QString pathString(const fs::path& path) {
  return QString::fromStdString(path.string());
}

// The width of a column that does not stretch.
constexpr int kNarrowColumnWidth = 110;

}  // namespace

MainWindow::MainWindow(Settings settings, fs::path builtInData)
    : settings_(std::move(settings)), readingFiller_(std::move(builtInData)) {
  learned_ = std::make_unique<LearnedVocabulary>(
      settings_.dataFolder, settings_.deviceID, settings_.hostName);
  QStringList phraseColumns{_("Phrase"), _("Reading")};
  pages_ = {
      {PageKind::kPhrases, _("Chinese Phrases"),
       _("Phrases added here come first in the candidates. The reading is "
         "filled in automatically, and can be changed for a character with "
         "more than one reading."),
       phraseColumns, phraseColumns, true, true, true},
      {PageKind::kExcluded, C_("Vocabulary", "Excluded Phrases"),
       _("Phrases listed here never appear in the candidates, such as "
         "excluding 你 to have 妳 first."),
       phraseColumns, phraseColumns, true, true, true},
      {PageKind::kEnglish, _("English Words"),
       _("Words typed twice or more, or added here, come first in the "
         "English completions with Tab."),
       {_("Word"), _("Times")}, {_("Word")}, false, false, false},
      {PageKind::kSentences, _("Common Sentences"),
       _("A sentence sent three times is remembered, and the rest of it is "
         "suggested after typing its beginning, to be taken with Tab. "
         "Sentences added here are remembered at once and never forgotten."),
       {_("Sentence"), _("Times"), _("Status")}, {_("Sentence")}, false, false,
       false},
      {PageKind::kTemporary, _("Temporary Phrases"),
       _("Phrases picked once come first for 30 days, and are added to the "
         "Chinese phrases when typed again."),
       {_("Phrase"), _("Reading"), _("Picked")}, {}, false, false, false},
      {PageKind::kTemplates, _("Text Templates"),
       _("Press ` and type the code to insert the text. Write \\n for a line "
         "break."),
       {_("Code"), _("Text")}, {_("Code"), _("Text")}, false, true, true},
  };

  setWindowTitle(_("Manage Vocabulary"));
  setMinimumSize(720, 560);
  resize(760, 600);

  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(16, 16, 16, 16);
  layout->setSpacing(10);

  tabs_ = new QTabBar(this);
  tabs_->setExpanding(false);
  tabs_->setDocumentMode(true);
  for (const auto& p : pages_) {
    tabs_->addTab(p.title);
  }
  layout->addWidget(tabs_);

  help_ = new QLabel(this);
  help_->setWordWrap(true);
  help_->setForegroundRole(QPalette::PlaceholderText);
  layout->addWidget(help_);

  notice_ = new QLabel(this);
  notice_->setWordWrap(true);
  notice_->setVisible(false);
  layout->addWidget(notice_);

  auto* searchRow = new QHBoxLayout();
  search_ = new QLineEdit(this);
  search_->setPlaceholderText(_("Search"));
  search_->setClearButtonEnabled(true);
  count_ = new QLabel(this);
  count_->setForegroundRole(QPalette::PlaceholderText);
  searchRow->addWidget(search_, 1);
  searchRow->addWidget(count_);
  layout->addLayout(searchRow);

  list_ = new QTreeWidget(this);
  list_->setRootIsDecorated(false);
  list_->setUniformRowHeights(true);
  list_->setSelectionMode(QAbstractItemView::ExtendedSelection);
  list_->setSortingEnabled(false);
  list_->setTextElideMode(Qt::ElideRight);
  list_->setWordWrap(false);
  list_->setMinimumHeight(260);
  list_->header()->setSectionsMovable(false);
  list_->header()->setStretchLastSection(false);
  layout->addWidget(list_, 1);

  editor_ = new QWidget(this);
  auto* editorRow = new QHBoxLayout(editor_);
  editorRow->setContentsMargins(0, 0, 0, 0);
  field1_ = new QLineEdit(editor_);
  field1_->setMaximumWidth(200);
  field2_ = new QLineEdit(editor_);
  addButton_ = new QPushButton(editor_);
  cancelButton_ = new QPushButton(_("Cancel"), editor_);
  editorRow->addWidget(field1_, 1);
  editorRow->addWidget(field2_, 2);
  editorRow->addWidget(addButton_);
  editorRow->addWidget(cancelButton_);
  layout->addWidget(editor_);

  auto* buttonRow = new QHBoxLayout();
  editButton_ = new QPushButton(_("Edit"), this);
  deleteButton_ = new QPushButton(_("Delete"), this);
  addToPhrasesButton_ = new QPushButton(_("Add to Chinese Phrases"), this);
  message_ = new QLabel(this);
  message_->setWordWrap(true);
  message_->setForegroundRole(QPalette::PlaceholderText);
  message_->setMaximumHeight(message_->fontMetrics().lineSpacing() * 2 + 4);
  revealButton_ = new QPushButton(_("Show in File Manager"), this);
  buttonRow->addWidget(editButton_);
  buttonRow->addWidget(deleteButton_);
  buttonRow->addWidget(addToPhrasesButton_);
  buttonRow->addWidget(message_, 1);
  buttonRow->addWidget(revealButton_);
  layout->addLayout(buttonRow);

  connect(tabs_, &QTabBar::currentChanged, this, &MainWindow::pageChanged);
  connect(search_, &QLineEdit::textChanged, this, [this] { showRows(); });
  connect(list_, &QTreeWidget::itemSelectionChanged, this, [this] {
    selection_.clear();
    for (auto* item : list_->selectedItems()) {
      selection_.insert(str(item->data(0, Qt::UserRole).toString()));
    }
    updateButtons();
  });
  connect(field1_, &QLineEdit::textEdited, this, &MainWindow::phraseEdited);
  connect(field2_, &QLineEdit::textEdited, this, &MainWindow::readingEdited);
  connect(field1_, &QLineEdit::returnPressed, this, &MainWindow::commit);
  connect(field2_, &QLineEdit::returnPressed, this, &MainWindow::commit);
  connect(addButton_, &QPushButton::clicked, this, &MainWindow::commit);
  connect(cancelButton_, &QPushButton::clicked, this,
          &MainWindow::cancelEditing);
  connect(editButton_, &QPushButton::clicked, this, &MainWindow::beginEditing);
  connect(deleteButton_, &QPushButton::clicked, this,
          &MainWindow::deleteSelected);
  connect(addToPhrasesButton_, &QPushButton::clicked, this,
          &MainWindow::addSelectedToPhrases);
  connect(revealButton_, &QPushButton::clicked, this, &MainWindow::revealFile);

  pageChanged(0);
}

MainWindow::~MainWindow() = default;

const MainWindow::Page& MainWindow::page() const { return pages_[pageIndex_]; }

std::optional<FileKind> MainWindow::pageFileKind() const {
  switch (page().kind) {
    case PageKind::kPhrases:
      return FileKind::kPhrases;
    case PageKind::kExcluded:
      return FileKind::kExcluded;
    case PageKind::kTemplates:
      return FileKind::kTemplates;
    default:
      return std::nullopt;
  }
}

std::optional<LearnedKind> MainWindow::pageLearnedKind() const {
  switch (page().kind) {
    case PageKind::kEnglish:
      return LearnedKind::kEnglish;
    case PageKind::kSentences:
      return LearnedKind::kSentences;
    case PageKind::kTemporary:
      return LearnedKind::kTemporary;
    default:
      return std::nullopt;
  }
}

fs::path MainWindow::pageFile() const {
  if (auto kind = pageFileKind()) {
    return vocabularyFile(settings_.dataFolder, *kind);
  }
  return {};
}

SentenceStatusTexts MainWindow::sentenceStatusTexts() const {
  return {str(_("Added")), str(_("Remembered")), [](long long times) {
            return str(_("%1 more times").arg(times));
          }};
}

int MainWindow::pageCount() const { return static_cast<int>(pages_.size()); }

QString MainWindow::pageTitle(int index) const { return pages_[index].title; }

void MainWindow::selectPage(int index) {
  if (index == tabs_->currentIndex()) {
    pageChanged(index);
  } else {
    tabs_->setCurrentIndex(index);
  }
}

void MainWindow::bringToFront() {
  reload();
  if (isMinimized()) {
    showNormal();
  } else {
    show();
  }
  raise();
  activateWindow();
}

void MainWindow::adoptSettings(Settings settings) {
  settings_ = std::move(settings);
  learned_ = std::make_unique<LearnedVocabulary>(
      settings_.dataFolder, settings_.deviceID, settings_.hostName);
  message_->clear();
  reload();
}

void MainWindow::preloadReadings() { readingFiller_.preload(); }

bool MainWindow::event(QEvent* event) {
  // The input method and other computers may change the files while the
  // window is open.
  if (event->type() == QEvent::WindowActivate) {
    reload();
  }
  return QWidget::event(event);
}

void MainWindow::pageChanged(int index) {
  if (index < 0 || index >= pageCount()) {
    return;
  }
  pageIndex_ = index;
  const Page& p = page();
  help_->setText(p.help);
  list_->setColumnCount(static_cast<int>(p.columns.size()));
  list_->setHeaderLabels(p.columns);
  // The first column, and the second where it is wide, stretch; the others
  // are narrow.
  for (int column = 0; column < static_cast<int>(p.columns.size());
       ++column) {
    if (column == 0 || (column == 1 && p.wideSecondColumn)) {
      list_->header()->setSectionResizeMode(column, QHeaderView::Stretch);
    } else {
      list_->header()->setSectionResizeMode(column, QHeaderView::Fixed);
      list_->header()->resizeSection(column, kNarrowColumnWidth);
    }
  }
  bool twoFields = p.fields.size() > 1;
  field1_->setPlaceholderText(p.fields.value(0));
  field1_->setMaximumWidth(twoFields ? 200 : QWIDGETSIZE_MAX);
  field2_->setPlaceholderText(p.fields.value(1));
  field2_->setVisible(twoFields);
  editor_->setVisible(!p.fields.isEmpty());
  editButton_->setVisible(p.canEdit);
  addToPhrasesButton_->setVisible(p.kind == PageKind::kTemporary);
  cancelEditing();
  selection_.clear();
  reload();
  list_->scrollToTop();
}

void MainWindow::loadRows() {
  if (auto kind = pageFileKind()) {
    rows_ = VocabularyManager::loadRows(*kind, pageFile());
  } else {
    rows_ = learned_->rows(*pageLearnedKind(), sentenceStatusTexts());
  }
}

void MainWindow::reload() {
  int scrolled = list_->verticalScrollBar()->value();
  loadRows();
  std::set<std::string> ids;
  for (const auto& row : rows_) {
    ids.insert(row.id);
  }
  std::set<std::string> kept;
  for (const auto& id : selection_) {
    if (ids.count(id) > 0) {
      kept.insert(id);
    }
  }
  selection_ = std::move(kept);
  updateNotice();
  showRows();
  list_->verticalScrollBar()->setValue(scrolled);
}

void MainWindow::showRows() {
  QString keyword = search_->text().trimmed();
  std::set<std::string> selection = selection_;
  list_->blockSignals(true);
  list_->clear();
  int shown = 0;
  QList<QTreeWidgetItem*> items;
  for (const auto& row : rows_) {
    QStringList columns;
    for (const auto& column : row.columns) {
      columns << qstr(column);
    }
    if (!keyword.isEmpty() &&
        std::none_of(columns.begin(), columns.end(), [&](const QString& c) {
          return c.contains(keyword, Qt::CaseInsensitive);
        })) {
      continue;
    }
    auto* item = new QTreeWidgetItem(columns);
    item->setData(0, Qt::UserRole, qstr(row.id));
    items << item;
    ++shown;
  }
  list_->addTopLevelItems(items);
  selection_.clear();
  for (auto* item : items) {
    std::string id = str(item->data(0, Qt::UserRole).toString());
    if (selection.count(id) > 0) {
      item->setSelected(true);
      selection_.insert(id);
    }
  }
  list_->blockSignals(false);
  count_->setText(_("%1 items").arg(shown));
  updateButtons();
}

void MainWindow::updateNotice() {
  QStringList notices;
  if (settings_.readOnly) {
    notices << _("The input method has not run yet, so the vocabulary can "
                 "only be viewed. Switch to McBopomofo once, then open this "
                 "window again.");
  }
  if (!settings_.warning.empty()) {
    notices << qstr(settings_.warning);
  }
  std::error_code ec;
  // The learned entries are in a folder of the user data folder.
  fs::path file = pageFileKind() ? pageFile() : settings_.dataFolder;
  if (!fs::exists(file, ec)) {
    notices << _("The file was not found. The vocabulary folder may still be "
                 "syncing.");
  }
  notice_->setText(notices.join("\n"));
  notice_->setVisible(!notices.isEmpty());
}

void MainWindow::updateButtons() {
  bool writable = !settings_.readOnly;
  field1_->setEnabled(writable);
  field2_->setEnabled(writable);
  addButton_->setEnabled(writable);
  addButton_->setText(editingKey_ ? _("Save") : _("Add"));
  cancelButton_->setVisible(editingKey_.has_value());
  editButton_->setEnabled(writable && selection_.size() == 1);
  deleteButton_->setEnabled(writable && !selection_.empty());
  addToPhrasesButton_->setEnabled(writable && !selection_.empty());
}

std::vector<const VocabularyRow*> MainWindow::selectedRows() const {
  std::vector<const VocabularyRow*> selected;
  for (const auto& row : rows_) {
    if (selection_.count(row.id) > 0) {
      selected.push_back(&row);
    }
  }
  return selected;
}

void MainWindow::phraseEdited() {
  if (!page().fillsReading || readingTyped_) {
    return;
  }
  // Empty when nothing is found; a reading with "？" is shown to be fixed,
  // and is refused when added.
  field2_->setText(qstr(readingFiller_.readingFor(str(field1_->text()))));
}

void MainWindow::readingEdited() { readingTyped_ = true; }

void MainWindow::beginEditing() {
  auto selected = selectedRows();
  if (settings_.readOnly || !page().canEdit || selected.size() != 1) {
    return;
  }
  const VocabularyRow& row = *selected.front();
  field1_->setText(qstr(row.columns[0]));
  field2_->setText(qstr(row.columns[1]));
  readingTyped_ = true;
  editingKey_ = row.key;
  message_->clear();
  updateButtons();
  field1_->setFocus();
}

void MainWindow::cancelEditing() {
  field1_->clear();
  field2_->clear();
  readingTyped_ = false;
  editingKey_.reset();
  updateButtons();
}

bool MainWindow::showFailure(CommitStatus status, const std::string& value) {
  switch (status) {
    case CommitStatus::kAdded:
    case CommitStatus::kSaved:
      return false;
    case CommitStatus::kNothingTyped:
    case CommitStatus::kReadOnly:
      QApplication::beep();
      break;
    case CommitStatus::kBadReading:
      message_->setText(
          _("Please type the reading in Bopomofo, with \"-\" between "
            "syllables, like ㄒㄧㄠˇ-ㄇㄞˋ."));
      break;
    case CommitStatus::kAlreadyListed:
      message_->setText(_("\"%1\" is already in the list.").arg(qstr(value)));
      break;
    case CommitStatus::kBadTemplate:
      message_->setText(_("Please type the code and the text."));
      break;
    case CommitStatus::kBadWord:
      message_->setText(_("An English word has two or more letters."));
      break;
    case CommitStatus::kBadSentence:
      message_->setText(_("A sentence has 6 to 40 characters or words, "
                          "without punctuation."));
      break;
    case CommitStatus::kFileMissing:
      message_->setText(_("The file was not found. The vocabulary folder may "
                          "still be syncing."));
      reload();
      break;
    case CommitStatus::kWriteFailed:
      message_->setText(_("The file could not be written."));
      break;
  }
  return true;
}

bool MainWindow::showFailure(DeleteStatus status) {
  switch (status) {
    case DeleteStatus::kDeleted:
      return false;
    case DeleteStatus::kNothingSelected:
    case DeleteStatus::kReadOnly:
      QApplication::beep();
      break;
    case DeleteStatus::kFileMissing:
      message_->setText(_("The file was not found. The vocabulary folder may "
                          "still be syncing."));
      reload();
      break;
    case DeleteStatus::kWriteFailed:
      message_->setText(_("The file could not be written."));
      reload();
      break;
  }
  return true;
}

void MainWindow::commit() {
  if (settings_.readOnly) {
    QApplication::beep();
    return;
  }
  bool editing = editingKey_.has_value();
  CommitResult result;
  if (auto kind = pageFileKind()) {
    result = commitEntry(*kind, pageFile(), str(field1_->text()),
                         str(field2_->text()), editingKey_);
  } else {
    result = learned_->add(*pageLearnedKind(), str(field1_->text()));
  }
  if (showFailure(result.status, result.value)) {
    return;
  }
  notifyInputMethod();
  message_->setText(editing ? _("Saved.")
                            : _("Added \"%1\".").arg(qstr(result.value)));
  cancelEditing();
  reload();
}

void MainWindow::deleteSelected() {
  std::vector<std::string> keys;
  for (const auto* row : selectedRows()) {
    keys.push_back(row->key);
  }
  if (settings_.readOnly) {
    QApplication::beep();
    return;
  }
  BatchResult result{DeleteStatus::kNothingSelected, 0};
  if (pageFileKind()) {
    result.status = deleteEntries(pageFile(), keys);
    result.done = result.status == DeleteStatus::kDeleted ? keys.size() : 0;
  } else {
    result = learned_->remove(*pageLearnedKind(), keys);
  }
  if (result.done > 0) {
    notifyInputMethod();
  }
  if (showFailure(result.status)) {
    return;
  }
  message_->setText(_("Deleted %1.").arg(static_cast<qulonglong>(keys.size())));
  selection_.clear();
  cancelEditing();
  reload();
}

void MainWindow::addSelectedToPhrases() {
  std::vector<std::string> keys;
  for (const auto* row : selectedRows()) {
    keys.push_back(row->key);
  }
  if (settings_.readOnly || page().kind != PageKind::kTemporary) {
    QApplication::beep();
    return;
  }
  BatchResult result = learned_->addToPhrases(keys);
  if (result.done > 0) {
    notifyInputMethod();
  }
  if (showFailure(result.status)) {
    return;
  }
  message_->setText(_("Added %1 to the Chinese phrases.")
                        .arg(static_cast<qulonglong>(keys.size())));
  selection_.clear();
  reload();
}

void MainWindow::revealFile() {
  // The learned entries are shown as their folder.
  fs::path file = pageFileKind() ? pageFile() : learned_->learningFolder();
  std::error_code ec;
  fs::path shown = fs::exists(file, ec) ? file : settings_.dataFolder;
  QDBusMessage call = QDBusMessage::createMethodCall(
      "org.freedesktop.FileManager1", "/org/freedesktop/FileManager1",
      "org.freedesktop.FileManager1", "ShowItems");
  call << QStringList{QUrl::fromLocalFile(pathString(shown)).toString()}
       << QString();
  QDBusMessage reply = QDBusConnection::sessionBus().call(call, QDBus::Block,
                                                          /*timeout=*/3000);
  if (reply.type() == QDBusMessage::ErrorMessage) {
    QDesktopServices::openUrl(
        QUrl::fromLocalFile(pathString(settings_.dataFolder)));
  }
}

void MainWindow::notifyInputMethod() {
  // The input method reads the files again; nothing happens when it is not
  // running, and it reads them when it starts.
  QDBusMessage call = QDBusMessage::createMethodCall(
      "org.fcitx.Fcitx5", "/controller", "org.fcitx.Fcitx.Controller1",
      "ReloadAddonConfig");
  call << QString("mcbopomofo");
  QDBusConnection::sessionBus().send(call);
}

}  // namespace McBopomofo::VocabularyManager
