#include <unistd.h>

#include <QApplication>
#include <QDir>
#include <QLocalServer>
#include <QLocalSocket>
#include <QStandardPaths>
#include <QTimer>
#include <cstdio>
#include <memory>
#include <string>
#include <string_view>
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

// Sends the arguments to the window already open, which comes to the front.
// False when no window answers or the arguments cannot be sent.
bool sendToOpenWindow(const QString& name,
                      const std::vector<std::string>& arguments) {
  QLocalSocket socket;
  socket.connectToServer(name);
  if (!socket.waitForConnected(500)) {
    return false;
  }
  std::string message =
      McBopomofo::VocabularyManager::encodeLaunchMessage(arguments);
  // Without arguments nothing is written, and the window only comes to the
  // front.
  socket.write(message.data(), static_cast<qint64>(message.size()));
  if (!message.empty() && !socket.waitForBytesWritten(1000)) {
    std::fprintf(stderr, "mcbopomofo-vocabulary: cannot reach the open "
                         "window (%s)\n",
                 socket.errorString().toLocal8Bit().constData());
    socket.abort();
    return false;
  }
  socket.disconnectFromServer();
  if (socket.state() != QLocalSocket::UnconnectedState) {
    socket.waitForDisconnected(1000);
  }
  return true;
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
  if (!options.selfTest && sendToOpenWindow(name, arguments)) {
    return 0;
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
  server.setSocketOptions(QLocalServer::UserAccessOption);
  QObject::connect(
      &server, &QLocalServer::newConnection, &window, [&server, &window, env] {
        while (QLocalSocket* client = server.nextPendingConnection()) {
          // The arguments of the second launch come until it disconnects.
          auto message = std::make_shared<QByteArray>();
          QObject::connect(client, &QLocalSocket::readyRead, client,
                           [client, message] { *message += client->readAll(); });
          QObject::connect(
              client, &QLocalSocket::disconnected, &window,
              [client, message, &window, env] {
                *message += client->readAll();
                client->deleteLater();
                auto arguments =
                    McBopomofo::VocabularyManager::decodeLaunchMessage(
                        std::string_view(message->constData(),
                                         static_cast<size_t>(message->size())));
                auto options =
                    McBopomofo::VocabularyManager::parseArguments(arguments);
                if (options.error.empty()) {
                  // Opened by the input method after being opened without a
                  // device ID, the window can now write; the input method's
                  // folder may also have changed.
                  if (auto adopted = McBopomofo::VocabularyManager::adoptLaunch(
                          window.currentSettings(), options, env,
                          hostName())) {
                    window.adoptSettings(std::move(*adopted));
                  }
                }
                window.bringToFront();
              });
        }
      });
  if (!server.listen(name)) {
    // Another launch may have started listening in the meantime.
    if (sendToOpenWindow(name, arguments)) {
      return 0;
    }
    // Left over by a window that did not quit normally.
    std::fprintf(stderr, "mcbopomofo-vocabulary: cannot listen on %s (%s); "
                         "trying again\n",
                 name.toLocal8Bit().constData(),
                 server.errorString().toLocal8Bit().constData());
    QLocalServer::removeServer(name);
    if (!server.listen(name)) {
      std::fprintf(stderr,
                   "mcbopomofo-vocabulary: cannot listen on %s (%s); opening "
                   "it again opens another window\n",
                   name.toLocal8Bit().constData(),
                   server.errorString().toLocal8Bit().constData());
    }
  }

  window.show();
  // Loaded once the window is up, rather than on the first phrase typed.
  QTimer::singleShot(0, &window, [&window] { window.preloadReadings(); });
  return QApplication::exec();
}
