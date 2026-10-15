#pragma once

#include <functional>

#include <QJsonObject>
#include <QLocalServer>
#include <QMenu>
#include <QNetworkAccessManager>
#include <QPointer>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QUrl>

#include "MainWindow.hpp"


namespace Huenicorn::Gui
{
  /**
   * @brief Desktop front end of the service: a tray icon for quick switches and a window with the web UI
   *
   * The lamps are driven by the service (systemd user unit), which keeps running without this application.
   */
  class DesktopApp : public QObject
  {
    Q_OBJECT

  public:
    /**
     * @brief DesktopApp constructor
     *
     * @param baseUrl Address of the service's web UI
     */
    explicit DesktopApp(const QUrl& baseUrl);


    /**
     * @brief Becomes the running instance, or hands a request over to the one already running
     *
     * @param showWindow Whether the window should be shown
     * @return true This process is the running instance
     * @return false Another instance runs and was notified
     */
    bool claimInstance(bool showWindow);


    /**
     * @brief Shows the window, creating it if needed
     *
     */
    void showWindow();


    /**
     * @brief Starts the service if it doesn't answer, and shows the tray icon if asked
     *
     * @param withTray Whether to show the tray icon
     */
    void start(bool withTray);


  private:
    using JsonHandler = std::function<void(const QJsonObject&)>;

    /**
     * @brief Sends a request to the service's API
     *
     * @param method HTTP method
     * @param path API path
     * @param body JSON body, empty for none
     * @param onSuccess Called with the JSON answer if the request succeeded
     */
    void _request(const QByteArray& method, const QString& path, const QByteArray& body = {}, JsonHandler onSuccess = {});


    void _buildMenu();


    /**
     * @brief Sets the menu texts in the current language
     *
     */
    void _retranslate();


    /**
     * @brief Updates the menu and the tooltip from the service state
     *
     */
    void _refresh();


    void _updateLightMode(const QJsonObject& lightMode);


    void _setReachable(bool reachable);


    void _startService();


    /**
     * @brief Shows the window if the current notice wasn't accepted yet
     *
     */
    void _checkNotice();


    // Attributes
    QUrl m_baseUrl;
    QNetworkAccessManager m_network;
    QLocalServer m_instanceServer;

    QSystemTrayIcon m_tray;
    QMenu m_menu;
    QAction* m_openAction{nullptr};
    QAction* m_syncAction{nullptr};
    QMenu* m_scenesMenu{nullptr};
    QAction* m_previewAction{nullptr};
    bool m_previewRunning{false};
    QAction* m_startServiceAction{nullptr};
    QAction* m_quitAction{nullptr};
    QTimer m_refreshTimer;

    QPointer<MainWindow> m_window;
  };
}
