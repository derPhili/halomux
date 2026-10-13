#include <Huenicorn/Grabber/DummyGrabber.hpp>

#include <glm/trigonometric.hpp>

#include <Huenicorn/Core/Config.hpp>


namespace Huenicorn::Grabber
{
  DummyGrabber::DummyGrabber(
    Core::Config* config
  ):
  IGrabber(config)
  {
    m_startTime = Timing::ClockType::now();
  }


  DummyGrabber::~DummyGrabber()
  {
  }


  const std::string& DummyGrabber::name() const
  {
    static const std::string s_identifier = "DummyGrabber";
    return s_identifier;
  }


  glm::ivec2 DummyGrabber::displayResolution() const
  {
    return m_resolution;
  }


  void DummyGrabber::grabFrameSubsample(
    Imaging::ImageData& imageData
  )
  {
    Timing::TimePoint now = Timing::ClockType::now();
    Timing::Duration duration = now - m_startTime;
    double seconds = duration.count();
    double max = 255.0;
    int rCoeff = static_cast<int>(((glm::sin(seconds / 2.0) + 1.0) / 2.0) * max);
    int gCoeff = static_cast<int>(((glm::sin(seconds / 3.0) + 1.0) / 2.0) * max);
    int bCoeff = static_cast<int>(((glm::sin(seconds / 5.0) + 1.0) / 2.0) * max);


    if(m_imageData.imageMatrix.empty()){
      m_imageData.imageMatrix.create(
        m_resolution.y,
        m_resolution.x,
        CV_8UC3
      );
    }

    m_imageData.imageMatrix.setTo(cv::Scalar(bCoeff, gCoeff, rCoeff));

    m_imageData.format = Imaging::PixelFormat::BGR;

    imageData = m_imageData;
  }


  IGrabber::RefreshRate DummyGrabber::displayRefreshRate() const
  {
    return m_refreshRate;
  }
}
