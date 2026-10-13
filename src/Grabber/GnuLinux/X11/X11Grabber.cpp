#include <Huenicorn/Grabber/GnuLinux/X11/X11Grabber.hpp>


#include <X11/Xutil.h>
#include <sys/shm.h>

#include <X11/extensions/Xrandr.h>

#include <Huenicorn/Core/Config.hpp>
#include <Huenicorn/Imaging/ImageProcessing.hpp>
#include <Huenicorn/Core/Logger.hpp>


namespace Huenicorn::Grabber
{
  // X11MonitorData
  X11Grabber::X11MonitorData::X11MonitorData(
    const std::string& name,
    unsigned width,
    unsigned height,
    double refreshRate,
    bool isPrimary,
    int xPos,
    int yPos,
    RROutput outputId
  ):
  MonitorData(name, width, height, refreshRate, isPrimary),
  xPos(xPos),
  yPos(yPos),
  outputId(outputId)
  {}


  // XShmData
  X11Grabber::XShmData::XShmData(
    Display* display,
    int screenId,
    unsigned width,
    unsigned height
  ):
  m_display(display)
  {
    m_shmInfo = std::make_unique<XShmSegmentInfo>();

    m_ximage.reset(XShmCreateImage(m_display,
      DefaultVisual(m_display, screenId),
      DefaultDepth(m_display, screenId),
      ZPixmap,
      nullptr,
      m_shmInfo.get(),
      width,
      height
    ));

    size_t size = static_cast<size_t>(m_ximage->bytes_per_line * m_ximage->height);

    m_shmInfo->shmid = shmget(IPC_PRIVATE, size, IPC_CREAT | 0777);
    m_shmInfo->readOnly = False;

    char* addr = reinterpret_cast<char*>(shmat(m_shmInfo->shmid, nullptr, 0));
    m_shmInfo->shmaddr = addr;
    m_ximage->data = addr;

    XShmAttach(m_display, m_shmInfo.get());
  }


  X11Grabber::XShmData::~XShmData()
  {
    shmdt(m_shmInfo->shmaddr);
    shmctl(m_shmInfo->shmid, IPC_RMID, 0);
    XShmDetach(m_display, m_shmInfo.get());
    m_shmInfo.reset();
  }


  XImage* X11Grabber::XShmData::ximage() const
  {
    return m_ximage.get();
  }


  // X11 Grabber
  X11Grabber::X11Grabber(
    Core::Config* config
  ):
  IGrabber(config)
  {
    _ensureXThreadsInit();

    m_display.reset(XOpenDisplay(nullptr));

    if(!m_display){
      throw std::runtime_error("Could not open any X11 display");
    }

    m_screenId = XDefaultScreen(m_display.get()); // Once and for all
  }


  const std::string& X11Grabber::name() const
  {
    static const std::string s_identifier = "X11Grabber";
    return s_identifier;
  }


  glm::ivec2 X11Grabber::displayResolution() const
  {
    try{
      auto selectedMonitor = m_monitorSelectionData.selectedMonitor();
      return {selectedMonitor->width, selectedMonitor->height};
    }
    catch(const std::exception& e){
      Core::Logger::error("No selected monitor");
    }

    return {0, 0};
  }


  IGrabber::RefreshRate X11Grabber::displayRefreshRate() const
  {
    try{
      auto selectedMonitor = m_monitorSelectionData.selectedMonitor();
      return static_cast<IGrabber::RefreshRate>(selectedMonitor->refreshRate);
    }
    catch(const std::exception& e){
      Core::Logger::error("No selected monitor");
    }

    return 0;
  }


  void X11Grabber::selectMonitor(
    unsigned monitorId
  )
  {
    m_xshmData.reset();
    m_monitorSelectionData.selectedMonitorId = monitorId;
    Core::Logger::log("selected ", m_monitorSelectionData.selectedMonitor()->name);
  }



  void X11Grabber::grabFrameSubsample(
    Imaging::ImageData& imageData
  )
  {
    if(!_ensureXShmData()){
      return;
    }

    auto* selectedMonitor = dynamic_cast<X11MonitorData*>(m_monitorSelectionData.selectedMonitor());

    int width = static_cast<int>(selectedMonitor->width);
    int height = static_cast<int>(selectedMonitor->height);
    auto ximage = m_xshmData->ximage();

    XShmGetImage(m_display.get(), RootWindow(m_display.get(), m_screenId), ximage, selectedMonitor->xPos, selectedMonitor->yPos, AllPlanes);

    // On little-endian hosts with the usual 0xff0000 red mask, pixels are laid out in memory as B, G, R(, X)
    bool bgrOrder = (ximage->red_mask == 0xff0000);

    int cvFormat;
    if(ximage->bits_per_pixel > 24){
      cvFormat = CV_8UC4;
      m_lastFullScreenFrame.format = bgrOrder ? Imaging::PixelFormat::BGRA : Imaging::PixelFormat::RGBA;
    }
    else{
      cvFormat = CV_8UC3;
      m_lastFullScreenFrame.format = bgrOrder ? Imaging::PixelFormat::BGR : Imaging::PixelFormat::RGB;
    }

    m_lastFullScreenFrame.imageMatrix = cv::Mat(height, width, cvFormat, ximage->data, static_cast<size_t>(ximage->bytes_per_line));

    imageData = m_lastFullScreenFrame;
  }


  void X11Grabber::_initMonitorsList()
  {
    MonitorSelectionData monitorSelectionData;

    if(!m_display.get()){
      throw std::runtime_error("No display available");
    }

    Window root = DefaultRootWindow(m_display.get());
    UniqueScreenResources screenResources(XRRGetScreenResources(m_display.get(), root));
    auto monitorsQuantity = screenResources->noutput;

    if(monitorsQuantity <= 0){
      throw std::runtime_error("No screen available");
    }

    RROutput primaryId = XRRGetOutputPrimary(m_display.get(), root);

    int minX = std::numeric_limits<int>::max();
    int minY = std::numeric_limits<int>::max();
    int maxX = std::numeric_limits<int>::min();
    int maxY = std::numeric_limits<int>::min();
    double minRefreshRate = 1000.0; // Let's not take any risk about this

    for(int i = 0; i < monitorsQuantity; i++){
      UniqueOutputInfo outputInfo(XRRGetOutputInfo(m_display.get(), screenResources.get(), screenResources->outputs[i]));

      if(outputInfo->connection == RR_Connected && outputInfo->crtc != 0){
        UniqueCrtcInfo crtcInfo(XRRGetCrtcInfo(m_display.get(), screenResources.get(), outputInfo->crtc));

        double refreshRate = 0.0;
        RRMode modeId = crtcInfo->mode;
        for(int i = 0; i < screenResources->nmode; ++i){
          if(screenResources->modes[i].id == modeId){
            const XRRModeInfo& mode = screenResources->modes[i];
            if(mode.hTotal && mode.vTotal){
              refreshRate = static_cast<double>(mode.dotClock) / (mode.hTotal * mode.vTotal);
            }
            break;
          }
        }

        minRefreshRate = std::min(refreshRate, minRefreshRate);

        int x = crtcInfo->x;
        int y = crtcInfo->y;
        int width = static_cast<int>(crtcInfo->width);
        int height = static_cast<int>(crtcInfo->height);
        bool isPrimary = (screenResources->outputs[i] == primaryId);

        monitorSelectionData.monitors.push_back(std::make_unique<X11MonitorData>(
          outputInfo->name,
          width,
          height,
          refreshRate,
          isPrimary,
          x,
          y,
          screenResources->outputs[i]
        ));

        minX = std::min(minX, x);
        minY = std::min(minY, y);
        maxX = std::max(maxX, x + width);
        maxY = std::max(maxY, y + height);

        if(isPrimary){
          monitorSelectionData.selectedMonitorId = monitorSelectionData.monitors.size() - 1;
        }
      }
    }

    if(monitorSelectionData.monitors.empty()){
      throw std::runtime_error("No monitor available");
    }

    if(!monitorSelectionData.selectedMonitorId.has_value()){
      Core::Logger::warn("No primary monitor reported by X11. Falling back to first monitor");
      monitorSelectionData.selectedMonitorId = 0;
    }


    // Add whole display surface to choices if there are multiple screens
    std::swap(m_monitorSelectionData, monitorSelectionData);
  }


  void X11Grabber::_ensureXThreadsInit()
  {
    static bool initialized = [](){
      return XInitThreads();
    }();

    if(!initialized){
      throw std::runtime_error("XInitThreads failed");
    }
  }


  bool X11Grabber::_ensureXShmData()
  {
    if(m_xshmData){
      return true;
    }

    if(auto monitor = m_monitorSelectionData.selectedMonitor()){
      m_xshmData = std::make_unique<XShmData>(m_display.get(), m_screenId, monitor->width, monitor->height);
      return true;
    }

    return false;
  }
}
