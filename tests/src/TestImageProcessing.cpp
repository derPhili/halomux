#include <filesystem>
#include <unordered_map>

#include <Huenicorn/Imaging/ImageProcessing.hpp>
#include <Huenicorn/Imaging/ImageData.hpp>
#include <Huenicorn/Core/Logger.hpp>
#include <Huenicorn/Timing/Definitions.hpp>

using namespace Huenicorn;


/**
 * @brief Checks that every supported pixel layout yields the same color once converted to BGR
 *
 * @return int Number of failures
 */
int testColorOrder()
{
  // Pure red, expressed in each layout
  const std::unordered_map<Imaging::PixelFormat, cv::Mat> inputs = {
    {Imaging::PixelFormat::RGB, cv::Mat(4, 4, CV_8UC3, cv::Scalar(255, 0, 0))},
    {Imaging::PixelFormat::BGR, cv::Mat(4, 4, CV_8UC3, cv::Scalar(0, 0, 255))},
    {Imaging::PixelFormat::RGBA, cv::Mat(4, 4, CV_8UC4, cv::Scalar(255, 0, 0, 255))},
    {Imaging::PixelFormat::BGRA, cv::Mat(4, 4, CV_8UC4, cv::Scalar(0, 0, 255, 255))}
  };

  int failures = 0;

  for(const auto& [format, matrix] : inputs){
    Imaging::ImageData imageData{.imageMatrix = matrix, .format = format};
    Imaging::ImageProcessing::toBgr(imageData, imageData);
    Imaging::Color color = Imaging::ImageProcessing::getDominantColor(imageData);

    if(color != Imaging::Color(255, 0, 0)){
      Core::Logger::error("Wrong color for pixel format ", static_cast<int>(format), ": ", static_cast<int>(color.m_r), " ", static_cast<int>(color.m_g), " ", static_cast<int>(color.m_b));
      failures++;
    }
  }

  return failures;
}


/**
 * @brief Checks rescaling edge cases
 *
 * @return int Number of failures
 */
int testRescaleBounds()
{
  int failures = 0;

  Imaging::ImageData imageData{.imageMatrix = cv::Mat(10, 1000, CV_8UC3, cv::Scalar(1, 2, 3)), .format = Imaging::PixelFormat::RGB};

  // Invalid width: input is kept as is
  Imaging::ImageData unchanged;
  Imaging::ImageProcessing::rescale(imageData, unchanged, 0, Imaging::Interpolation::Type::Area);
  if(unchanged.width() != imageData.width()){
    Core::Logger::error("Rescaling to a null width should keep the input");
    failures++;
  }

  // Very wide image: height must never be 0, format must be kept
  Imaging::ImageData resized;
  Imaging::ImageProcessing::rescale(imageData, resized, 10, Imaging::Interpolation::Type::Area);
  if(resized.width() != 10 || resized.height() < 1 || resized.format != Imaging::PixelFormat::RGB){
    Core::Logger::error("Unexpected rescale output ", resized.width(), "x", resized.height());
    failures++;
  }

  return failures;
}


void testResize(const std::filesystem::path& samplesDirPath)
{
  std::unordered_map<Imaging::Interpolation::Type, std::filesystem::path> testOutputs = {
    {Imaging::Interpolation::Type::Nearest, "Nearest"},
    {Imaging::Interpolation::Type::Cubic, "Cubic"},
    {Imaging::Interpolation::Type::Area, "Area"}
  };

  std::filesystem::path samplesInputDirPath = samplesDirPath / "Inputs";
  std::filesystem::path samplesOutputDirPath = samplesDirPath / "Outputs";

  if(!std::filesystem::is_directory(samplesInputDirPath)){
    Core::Logger::error("Invalid folder ", samplesInputDirPath);
    return;
  }

  std::filesystem::create_directory(samplesOutputDirPath);

  for(const auto& entry : std::filesystem::directory_iterator(samplesInputDirPath)){
    Imaging::ImageData imageData{.imageMatrix = cv::imread(entry.path().string()), .format = Imaging::PixelFormat::BGR};

    for(const auto& [type, interpolationOutDir] : testOutputs){
      std::filesystem::path fullpath = samplesOutputDirPath / std::filesystem::path(interpolationOutDir.string() + std::string("_") + entry.path().filename().string());

      Imaging::ImageData outputImageData;
      Imaging::ImageProcessing::rescale(imageData, outputImageData, imageData.width() / 10, type);

      Core::Logger::log("Saving ", fullpath);

      cv::imwrite(fullpath.string(), outputImageData.imageMatrix);
    }
  }
}


void testCrop(const std::filesystem::path& samplesDirPath)
{
  std::filesystem::path samplesInputDirPath = samplesDirPath / "Inputs";
  std::filesystem::path samplesOutputDirPath = samplesDirPath / "Outputs";

  if(!std::filesystem::is_directory(samplesInputDirPath)){
    Core::Logger::error("Invalid folder ", samplesInputDirPath);
    return;
  }

  std::filesystem::create_directory(samplesOutputDirPath);

  for(const auto& entry : std::filesystem::directory_iterator(samplesInputDirPath)){
    Imaging::ImageData imageData{.imageMatrix = cv::imread(entry.path().string()), .format = Imaging::PixelFormat::BGR};

    std::filesystem::path fullpath = samplesOutputDirPath / std::filesystem::path("crop_" + entry.path().filename().string());

    Imaging::ImageData outputImageData;
    Imaging::ImageProcessing::getSubImage(imageData, outputImageData, {{0.f, 0.f}, {0.5f, 0.5f}});

    Core::Logger::log("Saving ", fullpath);

    cv::imwrite(fullpath.string(), outputImageData.imageMatrix);
  }
}


void testPerf(const std::filesystem::path& samplesDirPath)
{
  std::filesystem::path samplesInputDirPath = samplesDirPath / "Inputs";

  auto entry = std::filesystem::directory_iterator(samplesInputDirPath);
  if(entry == std::filesystem::directory_iterator()){
    return;
  }

  Imaging::ImageData imageData{.imageMatrix = cv::imread(entry->path().string()), .format = Imaging::PixelFormat::BGR};

  Timing::TimePoint start = Timing::ClockType::now();
  Timing::TimePoint now = start;
  int i = 0;
  while(std::chrono::duration_cast<std::chrono::seconds>(now - start).count() < 3){
    Imaging::ImageData resampledImage;
    Imaging::ImageProcessing::rescale(imageData, resampledImage, imageData.width() / 10, Imaging::Interpolation::Type::Area);

    Imaging::ImageData croppedImage;
    Imaging::ImageProcessing::getSubImage(resampledImage, croppedImage, {{0.f, 0.f}, {0.05f, 0.05f}});

    now = Timing::ClockType::now();
    i++;
  }

  Core::Logger::log(i, " iterations in 3 seconds");
}


int main(int argc, char* argv[])
{
  int failures = testColorOrder() + testRescaleBounds();

  // Optional sample based tests, writing their outputs next to the inputs
  if(argc >= 2){
    std::filesystem::path samplesDirPath(argv[1]);

    testResize(samplesDirPath);
    testCrop(samplesDirPath);
    testPerf(samplesDirPath);
  }

  if(failures > 0){
    Core::Logger::error(failures, " test(s) failed");
    return 1;
  }

  Core::Logger::log("All tests passed");
  return 0;
}
