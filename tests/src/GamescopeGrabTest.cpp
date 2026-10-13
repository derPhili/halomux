#include <chrono>
#include <thread>
#include <filesystem>

#include <opencv2/opencv.hpp>

#include <Huenicorn/Core/Config.hpp>
#include <Huenicorn/Core/Logger.hpp>
#include <Huenicorn/Platform/Selector.hpp>
#include <Huenicorn/Imaging/ImageData.hpp>

using namespace Huenicorn;
using namespace std::chrono_literals;


int main(int, char**)
{
  Core::Config config("/tmp/huenicorn-test-config");
  config.setSubsampleWidth(32);

  auto* grabber = Platform::adapter.getGrabber(&config);
  Core::Logger::log("Grabber: ", grabber->name());

  try{
    const auto& res = grabber->displayResolution();
    Core::Logger::log("Display resolution: ", res.x, "x", res.y);
  }
  catch(const std::exception& e){
    Core::Logger::error("displayResolution() threw: ", e.what());
  }

  Imaging::ImageData imageData;
  int framesWithData = 0;
  int nonBlackFrames = 0;

  for(int i = 0; i < 100; ++i){
    grabber->grabFrameSubsample(imageData);

    if(imageData.hasData()){
      framesWithData++;
      cv::Scalar meanColor = cv::mean(imageData.imageMatrix);
      double intensity = meanColor[0] + meanColor[1] + meanColor[2];

      if(intensity > 1.0){
        nonBlackFrames++;
      }

      if(i % 10 == 0){
        Core::Logger::log("Frame ", i, ": ", imageData.width(), "x", imageData.height(), " mean=", meanColor[0], ",", meanColor[1], ",", meanColor[2], ",", meanColor[3]);
      }
    }

    std::this_thread::sleep_for(100ms);
  }

  Core::Logger::log("Frames with data: ", framesWithData, " / 100. Non-black frames: ", nonBlackFrames, ". Grabber used: ", grabber->name());

  return 0;
}
