#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QUrl>

#include <Halomux/Version.hpp>

#include "DesktopApp.hpp"
#include "Texts.hpp"

using namespace Halomux;


namespace
{
  // How long a start for the tray waits for the panel's tray to appear [s]
  constexpr int TrayWaitSeconds = 30;

  /**
   * @brief Returns the configuration of the service
   *
   * @return QJsonObject Content of its configuration file, empty if it can't be read
   */
  QJsonObject serviceConfig()
  {
    // Where the service keeps it: under $HOME/.config, whatever XDG_CONFIG_HOME says
    QFile configFile(QDir::homePath() + "/.config/" + AppId + "/config.json");
    if(!configFile.open(QIODevice::ReadOnly)){
      return {};
    }

    return QJsonDocument::fromJson(configFile.readAll()).object();
  }


  /**
   * @brief Returns the address of the service's web UI
   *
   * @param config Configuration of the service
   * @return QUrl Web UI address
   */
  QUrl serviceUrl(const QJsonObject& config)
  {
    QString address = "127.0.0.1";
    int port = 8215;

    if(!config.isEmpty()){
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

  const QJsonObject config = serviceConfig();
  Gui::Texts::setLanguage(config.value("language").toString("auto"));

  Gui::DesktopApp desktopApp(serviceUrl(config));
  if(!desktopApp.claimInstance(showWindow)){
    // The running instance shows its window
    return 0;
  }

  if(QSystemTrayIcon::isSystemTrayAvailable()){
    desktopApp.start(true);
    if(showWindow){
      desktopApp.showWindow();
    }
  }
  else if(showWindow){
    // Without a tray (e.g. GNOME without an extension), nothing could bring a closed window back
    QApplication::setQuitOnLastWindowClosed(true);
    desktopApp.start(false);
    desktopApp.showWindow();
  }
  else{
    // Started at login for the tray: the panel may still be starting, wait for it a little
    auto* trayWait = new QTimer(&app);
    QObject::connect(trayWait, &QTimer::timeout, &app, [&desktopApp, trayWait, waitedSeconds = 0]() mutable {
      if(QSystemTrayIcon::isSystemTrayAvailable()){
        trayWait->stop();
        desktopApp.start(true);
      }
      else if(++waitedSeconds >= TrayWaitSeconds){
        // No tray on this desktop: an invisible instance would be useless
        trayWait->stop();
        QApplication::quit();
      }
    });
    trayWait->start(1000);
  }

  return app.exec();
}
