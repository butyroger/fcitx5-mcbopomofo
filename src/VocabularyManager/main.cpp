#include <unistd.h>

#include <QApplication>
#include <QDir>
#include <QLocalServer>
#include <QLocalSocket>
#include <QStandardPaths>
#include <QTimer>
#include <cstdio>
#include <string>
#include <vector>

#include "FileVocabulary.h"
#include "MainWindow.h"
#include "ManagerSettings.h"
#include "Translation.h"

namespace {

using McBopomofo::VocabularyManager::MainWindow;

// The socket that the window listens on, so that opening the program again
// brings the open window to the front.
QString serverName() {
  QString folder =
      QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
  if (folder.isEmpty()) {
    folder = QDir::tempPath();
  }
  return folder + "/mcbopomofo-vocabulary-" + QString::number(getuid());
}

std::string hostName() {
  char name[256] = {};
  if (gethostname(name, sizeof(name) - 1) != 0) {
    name[0] = '\0';
  }
  return name;
}

// Opens every tab, prints how many rows each has, and fills in one reading.
int selfTest(MainWindow& window,
             McBopomofo::VocabularyManager::ReadingFiller& filler) {
  for (int i = 0; i < window.pageCount(); ++i) {
    window.selectPage(i);
    std::printf("%s: %zu\n", window.pageTitle(i).toStdString().c_str(),
                window.rowCount());
  }
  std::printf("reading: %s\n", filler.readingFor("台灣").c_str());
  return 0;
}

}  // namespace

int main(int argc, char* argv[]) {
  QApplication app(argc, argv);
  QApplication::setApplicationName("mcbopomofo-vocabulary");
  QApplication::setDesktopFileName("mcbopomofo-vocabulary");
  McBopomofo::VocabularyManager::initTranslation();

  std::vector<std::string> arguments;
  const QStringList appArguments = QApplication::arguments();
  for (qsizetype i = 1; i < appArguments.size(); ++i) {
    arguments.push_back(appArguments[i].toStdString());
  }
  auto options = McBopomofo::VocabularyManager::parseArguments(arguments);
  if (!options.error.empty()) {
    std::fprintf(stderr,
                 "Cannot use the argument: %s\n"
                 "Usage: mcbopomofo-vocabulary [--data-folder <folder>] "
                 "[--device-id <id>]\n",
                 options.error.c_str());
    return 2;
  }

  QString name = serverName();
  if (!options.selfTest) {
    QLocalSocket socket;
    socket.connectToServer(name);
    if (socket.waitForConnected(500)) {
      // The window is already open; it comes to the front.
      socket.write("show\n");
      socket.waitForBytesWritten(500);
      socket.disconnectFromServer();
      return 0;
    }
  }

  auto env = McBopomofo::VocabularyManager::processEnvironment();
  auto settings =
      McBopomofo::VocabularyManager::resolveSettings(options, env, hostName());
  auto builtInData = McBopomofo::VocabularyManager::findBuiltInData(
      env, MCBOPOMOFO_PKGDATADIR);
  MainWindow window(settings, builtInData);

  if (options.selfTest) {
    window.show();
    McBopomofo::VocabularyManager::ReadingFiller filler(builtInData);
    int result = selfTest(window, filler);
    QTimer::singleShot(0, &app, [&app, result] { app.exit(result); });
    return QApplication::exec();
  }

  QLocalServer server;
  QLocalServer::removeServer(name);
  server.setSocketOptions(QLocalServer::UserAccessOption);
  QObject::connect(&server, &QLocalServer::newConnection, &window,
                   [&server, &window] {
                     while (QLocalSocket* client =
                                server.nextPendingConnection()) {
                       QObject::connect(client, &QLocalSocket::disconnected,
                                        client, &QObject::deleteLater);
                       window.bringToFront();
                     }
                   });
  server.listen(name);

  window.show();
  return QApplication::exec();
}
