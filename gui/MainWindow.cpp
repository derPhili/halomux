#include "MainWindow.hpp"

#include <QCloseEvent>
#include <QDesktopServices>
#include <QSettings>
#include <QTimer>
#include <QWebEngineNewWindowRequest>
#include <QWebEnginePage>

#include <Halomux/Version.hpp>

#include "Texts.hpp"


namespace Halomux::Gui
{
  namespace
  {
    // Interval between connection attempts while the service doesn't answer [ms]
    constexpr int RetryInterval = 3000;

    // %1: title, %2: explanation
    const char* UnreachableNotice = R"(<!DOCTYPE html>
<html>
  <body style="margin: 0; height: 100vh; display: flex; align-items: center; justify-content: center;
               background: #0e1014; color: #9097a4; font: 14px system-ui, sans-serif; text-align: center;">
    <div>
      <p style="color: #e7e9ee; font-size: 16px; margin-bottom: 8px;">%1</p>
      <p>%2</p>
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

    // Links opening a new window, such as the one to the source code, open in the browser
    connect(page(), &QWebEnginePage::newWindowRequested, this, [](QWebEngineNewWindowRequest& request){
      QDesktopServices::openUrl(request.requestedUrl());
    });

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
    setHtml(QString(UnreachableNotice).arg(
      Texts::tr("The service isn't running").toHtmlEscaped(),
      Texts::tr("Start it from the tray menu. This window connects as soon as it answers.").toHtmlEscaped()
    ));
  }
}
