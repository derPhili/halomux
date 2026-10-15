#include "DesktopApp.hpp"

#include <QApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLocalSocket>
#include <QNetworkReply>
#include <QProcess>

#include <Huenicorn/Version.hpp>

#include "Texts.hpp"


namespace Huenicorn::Gui
{
  namespace
  {
    // Interval between tray state updates [ms]
    constexpr int RefreshInterval = 10000;

    QString instanceName()
    {
      return QString("%1-gui-%2").arg(AppId, qEnvironmentVariable("USER"));
    }
  }


  DesktopApp::DesktopApp(const QUrl& baseUrl):
  m_baseUrl(baseUrl)
  {
    _buildMenu();

    m_tray.setIcon(QApplication::windowIcon());
    m_tray.setToolTip(AppName);
    m_tray.setContextMenu(&m_menu);

    connect(&m_tray, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason){
      if(reason != QSystemTrayIcon::Trigger){
        return;
      }

      if(m_window && m_window->isVisible() && m_window->isActiveWindow()){
        m_window->close();
      }
      else{
        showWindow();
      }
    });

    connect(&m_refreshTimer, &QTimer::timeout, this, &DesktopApp::_refresh);
  }


  void DesktopApp::start(bool withTray)
  {
    if(withTray){
      m_tray.show();
      m_refreshTimer.start(RefreshInterval);
    }

    // Started from the desktop while the service is stopped: start it
    QNetworkReply* reply = m_network.get(QNetworkRequest(m_baseUrl.resolved(QUrl("/api/version"))));
    connect(reply, &QNetworkReply::finished, this, [this, reply](){
      if(reply->error() != QNetworkReply::NoError){
        _startService();
      }
      else{
        _refresh();
        _checkNotice();
      }

      reply->deleteLater();
    });
  }


  void DesktopApp::_checkNotice()
  {
    // The light manager covers itself with the notice until it's accepted: show it, even when
    // started for the tray, so it isn't missed. During the setup, which asks for it itself,
    // the request fails and nothing happens
    _request("GET", "/api/notice", {}, [this](const QJsonObject& notice){
      if(!notice.value("accepted").toBool(true)){
        showWindow();
      }
    });
  }


  bool DesktopApp::claimInstance(bool showWindow)
  {
    QLocalSocket socket;
    socket.connectToServer(instanceName());
    if(socket.waitForConnected(500)){
      socket.write(showWindow ? "show" : "tray");
      socket.waitForBytesWritten(500);
      return false;
    }

    // Left over by an instance that crashed
    QLocalServer::removeServer(instanceName());

    connect(&m_instanceServer, &QLocalServer::newConnection, this, [this](){
      QLocalSocket* client = m_instanceServer.nextPendingConnection();
      connect(client, &QLocalSocket::readyRead, this, [this, client](){
        if(client->readAll() == "show"){
          this->showWindow();
        }
        client->deleteLater();
      });
    });

    m_instanceServer.listen(instanceName());
    return true;
  }


  void DesktopApp::showWindow()
  {
    if(!m_window){
      m_window = new MainWindow(m_baseUrl);
    }

    m_window->show();
    m_window->raise();
    m_window->activateWindow();
  }


  void DesktopApp::_request(const QByteArray& method, const QString& path, const QByteArray& body, JsonHandler onSuccess)
  {
    QNetworkRequest request(m_baseUrl.resolved(QUrl(path)));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkReply* reply = m_network.sendCustomRequest(request, method, body);
    connect(reply, &QNetworkReply::finished, this, [this, reply, onSuccess = std::move(onSuccess)](){
      const bool reachable = reply->error() == QNetworkReply::NoError || reply->error() > QNetworkReply::UnknownNetworkError;
      _setReachable(reachable);

      if(reply->error() == QNetworkReply::NoError && onSuccess){
        onSuccess(QJsonDocument::fromJson(reply->readAll()).object());
      }

      reply->deleteLater();
    });
  }


  void DesktopApp::_buildMenu()
  {
    m_openAction = m_menu.addAction(QIcon::fromTheme("window-new"), QString());
    connect(m_openAction, &QAction::triggered, this, &DesktopApp::showWindow);
    m_menu.setDefaultAction(m_openAction);

    m_menu.addSeparator();

    m_syncAction = m_menu.addAction(QString());
    m_syncAction->setCheckable(true);
    connect(m_syncAction, &QAction::triggered, this, [this](bool checked){
      _request("PUT", "/api/setSyncEnabled", checked ? "true" : "false", [this](const QJsonObject&){_refresh();});
    });

    m_scenesMenu = m_menu.addMenu(QIcon::fromTheme("color-management"), QString());

    m_previewAction = m_menu.addAction(QIcon::fromTheme("media-playback-start"), QString());
    connect(m_previewAction, &QAction::triggered, this, [this](){
      // Stops a running preview, starts one otherwise
      const QByteArray method = m_previewRunning ? "DELETE" : "POST";
      _request(method, "/api/preview", "null", [this](const QJsonObject&){_refresh();});
    });

    m_startServiceAction = m_menu.addAction(QIcon::fromTheme("system-run"), QString());
    m_startServiceAction->setVisible(false);
    connect(m_startServiceAction, &QAction::triggered, this, &DesktopApp::_startService);

    m_menu.addSeparator();

    m_quitAction = m_menu.addAction(QIcon::fromTheme("application-exit"), QString());
    connect(m_quitAction, &QAction::triggered, qApp, &QApplication::quit);

    connect(&m_menu, &QMenu::aboutToShow, this, &DesktopApp::_refresh);

    _retranslate();
  }


  void DesktopApp::_retranslate()
  {
    m_openAction->setText(Texts::tr("Open"));
    m_syncAction->setText(Texts::tr("Sync with screen and sound"));
    m_scenesMenu->setTitle(Texts::tr("Static scene"));
    m_previewAction->setText(Texts::tr(m_previewRunning ? "Stop preview" : "Preview settings"));
    m_startServiceAction->setText(Texts::tr("Start service"));
    m_quitAction->setText(Texts::tr("Close tray icon"));
  }


  void DesktopApp::_refresh()
  {
    _request("GET", "/api/lightMode", {}, [this](const QJsonObject& lightMode){_updateLightMode(lightMode);});
  }


  void DesktopApp::_updateLightMode(const QJsonObject& lightMode)
  {
    // The language is shared with the web UI, which can change it
    if(Texts::setLanguage(lightMode.value("language").toString())){
      _retranslate();
    }

    m_previewRunning = lightMode.value("previewRemaining").toDouble() > 0.;
    m_previewAction->setText(Texts::tr(m_previewRunning ? "Stop preview" : "Preview settings"));
    m_previewAction->setIcon(QIcon::fromTheme(m_previewRunning ? "media-playback-stop" : "media-playback-start"));

    const bool syncEnabled = lightMode.value("syncEnabled").toBool();
    m_syncAction->setChecked(syncEnabled);
    m_tray.setToolTip(QString("%1\n%2").arg(AppName, Texts::tr(syncEnabled ? "Sync on" : "Static scene")));

    m_scenesMenu->clear();

    auto addScene = [this](const QString& name, bool builtin){
      // Built-in scene names are translated, saved ones are the user's
      QAction* action = m_scenesMenu->addAction(builtin ? Texts::tr(name) : name);
      connect(action, &QAction::triggered, this, [this, name, builtin](){
        const QJsonObject scene{{"name", name}, {"builtin", builtin}};
        _request("PUT", "/api/applyScene", QJsonDocument(scene).toJson(QJsonDocument::Compact), [this](const QJsonObject&){_refresh();});
      });
    };

    for(const QJsonValue& scene : lightMode.value("builtinScenes").toArray()){
      addScene(scene.toObject().value("name").toString(), true);
    }

    const QJsonObject savedScenes = lightMode.value("scenes").toObject();
    if(!savedScenes.isEmpty()){
      m_scenesMenu->addSection(Texts::tr("My scenes"));
      for(const QString& name : savedScenes.keys()){
        addScene(name, false);
      }
    }
  }


  void DesktopApp::_setReachable(bool reachable)
  {
    m_syncAction->setEnabled(reachable);
    m_scenesMenu->setEnabled(reachable);
    m_previewAction->setEnabled(reachable);
    m_startServiceAction->setVisible(!reachable);

    if(!reachable){
      m_tray.setToolTip(QString("%1\n%2").arg(AppName, Texts::tr("Service not running")));
    }
  }


  void DesktopApp::_startService()
  {
    QProcess::startDetached("systemctl", {"--user", "start", QString("%1.service").arg(AppId)});
    QTimer::singleShot(3000, this, [this](){
      _refresh();
      _checkNotice();
    });
  }
}
