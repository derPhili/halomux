#include <algorithm>
#include <cstdlib>
#include <filesystem>

#include <fcntl.h>
#include <unistd.h>

#include <drm_fourcc.h>
#include <gbm.h>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <Huenicorn/Core/Logger.hpp>
#include <Huenicorn/Grabber/GnuLinux/Pipewire/EglDownscaler.hpp>
#include <Huenicorn/Imaging/ImageProcessing.hpp>

using namespace Huenicorn;


/**
 * @brief Downscales a known image through a real DMA-BUF on the GPU and compares it with an
 * exact CPU area average
 *
 * Usage: TestGpuDownscale <image> [render node]
 */
int main(int argc, char* argv[])
{
  if(argc < 2){
    Core::Logger::error("Usage: TestGpuDownscale <image> [render node]");
    return 1;
  }

  const char* renderNode = argc >= 3 ? argv[2] : "/dev/dri/renderD128";

  // Source frame: the image scaled to a 2560x1440 BGRx frame, like the screencast
  cv::Mat image = cv::imread(argv[1]);
  if(image.empty()){
    Core::Logger::error("Could not read ", argv[1]);
    return 1;
  }

  cv::Mat frame;
  cv::resize(image, frame, cv::Size(2560, 1440), 0, 0, cv::INTER_CUBIC);
  cv::cvtColor(frame, frame, cv::COLOR_BGR2BGRA);

  // Linear DMA-BUF holding the frame
  int drmFd = open(renderNode, O_RDWR | O_CLOEXEC);
  if(drmFd < 0){
    Core::Logger::error("Could not open ", renderNode);
    return 1;
  }

  gbm_device* device = gbm_create_device(drmFd);
  gbm_bo* bo = device ? gbm_bo_create(device, frame.cols, frame.rows, GBM_FORMAT_XRGB8888, GBM_BO_USE_LINEAR) : nullptr;
  if(!bo){
    Core::Logger::error("Could not allocate a linear GBM buffer");
    return 1;
  }

  uint32_t mapStride = 0;
  void* mapData = nullptr;
  auto* mapped = static_cast<uint8_t*>(gbm_bo_map(bo, 0, 0, frame.cols, frame.rows, GBM_BO_TRANSFER_WRITE, &mapStride, &mapData));
  if(!mapped){
    Core::Logger::error("Could not map the GBM buffer");
    return 1;
  }

  for(int y = 0; y < frame.rows; y++){
    std::memcpy(mapped + static_cast<size_t>(y) * mapStride, frame.ptr(y), static_cast<size_t>(frame.cols) * 4);
  }
  gbm_bo_unmap(bo, mapData);

  int dmaBufFd = gbm_bo_get_fd(bo);

  // GPU downscale
  Grabber::EglDownscaler downscaler;
  if(!downscaler.init()){
    Core::Logger::error("GPU downscaler unavailable");
    return 1;
  }

  auto gpuImage = downscaler.importDmaBuf({
    .fd = dmaBufFd,
    .offset = gbm_bo_get_offset(bo, 0),
    .stride = gbm_bo_get_stride(bo),
    .width = frame.cols,
    .height = frame.rows,
    .drmFormat = DRM_FORMAT_XRGB8888,
    .modifier = DRM_FORMAT_MOD_LINEAR
  });

  if(!gpuImage){
    return 1;
  }

  int failures = 0;

  for(int outputWidth : {160, 80, 320}){
    const int outputHeight = frame.rows * outputWidth / frame.cols;

    cv::Mat gpuRgba;
    if(!downscaler.downscale(*gpuImage, outputWidth, outputHeight, gpuRgba)){
      return 1;
    }

    cv::Mat gpuBgr;
    cv::cvtColor(gpuRgba, gpuBgr, cv::COLOR_RGBA2BGR);

    // Exact area average over every pixel
    cv::Mat reference;
    cv::resize(frame, reference, cv::Size(outputWidth, outputHeight), 0, 0, cv::INTER_AREA);
    cv::cvtColor(reference, reference, cv::COLOR_BGRA2BGR);

    cv::Mat difference;
    cv::absdiff(gpuBgr, reference, difference);
    double maxDifference = 0;
    cv::minMaxLoc(difference.reshape(1), nullptr, &maxDifference);
    cv::Scalar meanDifference = cv::mean(difference);

    Core::Logger::log(outputWidth, "x", outputHeight, ": max pixel difference ", maxDifference, ", mean ", (meanDifference[0] + meanDifference[1] + meanDifference[2]) / 3);

    // Rounding differences only
    if(maxDifference > 2){
      Core::Logger::error("GPU result differs from the CPU area average");
      failures++;
    }
  }

  // Region averages: GPU (subsample + regions stages) against the CPU path on an exact subsample
  {
    const int subsampleWidth = 160;
    const int subsampleHeight = frame.rows * subsampleWidth / frame.cols;

    const std::vector<Imaging::UVs> regions = {
      {{0.f, 0.f}, {1.f, 1.f}},       // Whole screen
      {{0.f, 0.f}, {0.5f, 1.f}},      // Left half
      {{0.5f, 0.f}, {1.f, 1.f}},      // Right half
      {{0.1f, 0.2f}, {0.13f, 0.27f}}, // Small region
      {{0.4f, 0.4f}, {0.4f, 0.6f}}    // Empty region
    };

    std::vector<cv::Rect> rects;
    for(const auto& uvs : regions){
      rects.push_back(Imaging::ImageProcessing::subImageRect(subsampleWidth, subsampleHeight, uvs));
    }

    std::vector<cv::Vec4b> gpuColors;
    if(!downscaler.averageRegions(*gpuImage, subsampleWidth, subsampleHeight, rects, gpuColors)){
      return 1;
    }

    cv::Mat reference;
    cv::resize(frame, reference, cv::Size(subsampleWidth, subsampleHeight), 0, 0, cv::INTER_AREA);
    Imaging::ImageData referenceData{.imageMatrix = reference, .format = Imaging::PixelFormat::BGRA};
    Imaging::ImageProcessing::toBgr(referenceData, referenceData);
    auto cpuColors = Imaging::ImageProcessing::averageRegions(referenceData, regions);

    for(size_t i = 0; i < regions.size(); i++){
      const cv::Vec4b& gpu = gpuColors[i];
      const Imaging::Color& cpu = cpuColors[i];
      const int difference = std::max({std::abs(gpu[0] - cpu.m_r), std::abs(gpu[1] - cpu.m_g), std::abs(gpu[2] - cpu.m_b)});

      Core::Logger::log("Region ", i, ": GPU ", int(gpu[0]), " ", int(gpu[1]), " ", int(gpu[2]), ", CPU ", int(cpu.m_r), " ", int(cpu.m_g), " ", int(cpu.m_b));

      // The GPU subsample may differ from the exact one by one step per pixel
      if(difference > 1){
        Core::Logger::error("GPU region average differs from the CPU one");
        failures++;
      }
    }
  }

  downscaler.releaseImage(*gpuImage);
  close(dmaBufFd);
  gbm_bo_destroy(bo);
  gbm_device_destroy(device);
  close(drmFd);

  if(failures > 0){
    return 1;
  }

  Core::Logger::log("GPU downscale matches the CPU area average");
  return 0;
}
