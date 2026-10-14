#include "MainWindow.hpp"

#include <QCloseEvent>
#include <QSettings>
#include <QTimer>

#include <Huenicorn/Version.hpp>


namespace Huenicorn::Gui
{
  namespace
  {
    // Interval between connection attempts while the service doesn't answer [ms]
    constexpr int RetryInterval = 3000;

    const char* UnreachableNotice = R"(<!DOCTYPE html>
<html>
  <body style="margin: 0; height: 100vh; display: flex; align-items: center; justify-content: center;
               background: #0e1014; color: #9097a4; font: 14px system-ui, sans-serif; text-align: center;">
    <div>
      <p style="color: #e7e9ee; font-size: 16px; margin-bottom: 8px;">The service isn't running</p>
      <p>Start it from the tray menu. This window connects as soon as it answers.</p>
    </div>
  </body>
</html>)";
  }


  MainWindow::MainWindow(const QUrl& url):
  m_url(url)
  {
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(AppName);
    setMinimumSize(720, 520);

    QSettings settings;
    if(!restoreGeometry(settings.value("window/geometry").toByteArray())){
      resize(1440, 1000);
    }

    connect(this, &QWebEngineView::loadFinished, this, [this](bool ok){
      if(m_showingNotice){
        // The notice itself was loaded: try the service again later
        m_showingNotice = false;
        QTimer::singleShot(RetryInterval, this, [this](){load(m_url);});
        return;
      }

      if(!ok){
        _showUnreachable();
      }
    });

    load(m_url);
  }


  void MainWindow::closeEvent(QCloseEvent* event)
  {
    QSettings settings;
    settings.setValue("window/geometry", saveGeometry());

    // Deleted on close: the web engine only uses memory while the window is open
    event->accept();
  }


  void MainWindow::_showUnreachable()
  {
    m_showingNotice = true;
    setHtml(UnreachableNotice);
  }
}
