#include <QApplication>
#include <QCommandLineParser>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QUrl>

#include <Huenicorn/Version.hpp>

#include "DesktopApp.hpp"

using namespace Huenicorn;


namespace
{
  /**
   * @brief Returns the address of the service's web UI, from its configuration file
   *
   * @return QUrl Web UI address
   */
  QUrl serviceUrl()
  {
    QString address = "127.0.0.1";
    int port = 8215;

    QFile configFile(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + "/" + AppId + "/config.json");
    if(configFile.open(QIODevice::ReadOnly)){
      const QJsonObject config = QJsonDocument::fromJson(configFile.readAll()).object();
      port = config.value("restServerPort").toInt(port);

      // A service bound to every interface also answers locally
      const QString boundAddress = config.value("boundBackendIP").toString();
      if(!boundAddress.isEmpty() && boundAddress != "0.0.0.0"){
        address = boundAddress;
      }
    }

    QUrl url;
    url.setScheme("http");
    url.setHost(address);
    url.setPort(port);
    url.setPath("/");
    return url;
  }
}


int main(int argc, char* argv[])
{
  QApplication app(argc, argv);
  QApplication::setApplicationName(AppId);
  QApplication::setApplicationDisplayName(AppName);
  QApplication::setApplicationVersion(QString::fromStdString(Version));
  QApplication::setOrganizationName(AppId);
  // Ties the windows to the desktop entry: name and icon in the task manager
  QApplication::setDesktopFileName(AppId);
  QApplication::setWindowIcon(QIcon::fromTheme(AppId, QIcon(":/logo.svg")));
  // Closing the window leaves the tray icon
  QApplication::setQuitOnLastWindowClosed(false);

  QCommandLineParser parser;
  parser.setApplicationDescription(AppName);
  parser.addHelpOption();
  parser.addVersionOption();
  QCommandLineOption trayOption("tray", "Start in the tray, without opening the window");
  parser.addOption(trayOption);
  parser.process(app);

  const bool showWindow = !parser.isSet(trayOption);

  Gui::DesktopApp desktopApp(serviceUrl());
  if(!desktopApp.claimInstance(showWindow)){
    // The running instance shows its window
    return 0;
  }

  desktopApp.start();

  if(showWindow){
    desktopApp.showWindow();
  }

  return app.exec();
}
