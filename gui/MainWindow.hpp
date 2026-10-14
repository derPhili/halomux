#pragma once

#include <QUrl>
#include <QWebEngineView>


namespace Huenicorn::Gui
{
  /**
   * @brief Window showing the web UI of the service, without a browser
   *
   */
  class MainWindow : public QWebEngineView
  {
    Q_OBJECT

  public:
    /**
     * @brief MainWindow constructor
     *
     * @param url Address of the web UI
     */
    explicit MainWindow(const QUrl& url);


  protected:
    void closeEvent(QCloseEvent* event) override;


  private:
    /**
     * @brief Shows a notice while the service doesn't answer, and tries again a bit later
     *
     */
    void _showUnreachable();


    // Attributes
    QUrl m_url;
    bool m_showingNotice{false};
  };
}
