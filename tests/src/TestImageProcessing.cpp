#include <cmath>
#include <filesystem>
#include <unordered_map>

#include <Halomux/Imaging/ImageProcessing.hpp>
#include <Halomux/Imaging/ImageData.hpp>
#include <Halomux/Core/Logger.hpp>
#include <Halomux/Timing/Definitions.hpp>

using namespace Halomux;


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
  Imaging::ImageProcessing::rescale(imageData, unchanged, 0);
  if(unchanged.width() != imageData.width()){
    Core::Logger::error("Rescaling to a null width should keep the input");
    failures++;
  }

  // Very wide image: height must never be 0, format must be kept
  Imaging::ImageData resized;
  Imaging::ImageProcessing::rescale(imageData, resized, 10);
  if(resized.width() != 10 || resized.height() < 1 || resized.format != Imaging::PixelFormat::RGB){
    Core::Logger::error("Unexpected rescale output ", resized.width(), "x", resized.height());
    failures++;
  }

  return failures;
}


/**
 * @brief Checks that the row-sampled rescale keeps the full image geometry and averages
 *
 * @return int Number of failures
 */
int testRescaleSampled()
{
  int failures = 0;

  // Vertical gradient: sampling every 4th row must keep the output close to a full rescale
  cv::Mat gradient(1440, 2560, CV_8UC4);
  for(int y = 0; y < gradient.rows; y++){
    gradient.row(y).setTo(cv::Scalar(y % 256, (y / 6) % 256, 255 - y % 256, 255));
  }

  Imaging::ImageData imageData{.imageMatrix = gradient, .format = Imaging::PixelFormat::BGRA};

  Imaging::ImageData full;
  Imaging::ImageProcessing::rescale(imageData, full, 160);

  Imaging::ImageData sampled;
  Imaging::ImageProcessing::rescaleSampled(imageData, sampled, 160);

  if(sampled.width() != full.width() || sampled.height() != full.height() || sampled.format != full.format){
    Core::Logger::error("Sampled rescale geometry mismatch: ", sampled.width(), "x", sampled.height(), " instead of ", full.width(), "x", full.height());
    return 1;
  }

  cv::Scalar fullMean = cv::mean(full.imageMatrix);
  cv::Scalar sampledMean = cv::mean(sampled.imageMatrix);
  for(int c = 0; c < 3; c++){
    if(std::abs(fullMean[c] - sampledMean[c]) > 2.0){
      Core::Logger::error("Sampled rescale mean differs on channel ", c, ": ", sampledMean[c], " instead of ", fullMean[c]);
      failures++;
    }
  }

  return failures;
}


void testResize(const std::filesystem::path& samplesDirPath)
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

    std::filesystem::path fullpath = samplesOutputDirPath / std::filesystem::path("resized_" + entry.path().filename().string());

    Imaging::ImageData outputImageData;
    Imaging::ImageProcessing::rescale(imageData, outputImageData, imageData.width() / 10);

    Core::Logger::log("Saving ", fullpath);

    cv::imwrite(fullpath.string(), outputImageData.imageMatrix);
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
    Imaging::ImageProcessing::rescale(imageData, resampledImage, imageData.width() / 10);

    Imaging::ImageData croppedImage;
    Imaging::ImageProcessing::getSubImage(resampledImage, croppedImage, {{0.f, 0.f}, {0.05f, 0.05f}});

    now = Timing::ClockType::now();
    i++;
  }

  Core::Logger::log(i, " iterations in 3 seconds");
}


int main(int argc, char* argv[])
{
  int failures = testColorOrder() + testRescaleBounds() + testRescaleSampled();

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
