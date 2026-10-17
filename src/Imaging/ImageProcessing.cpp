#include <Halomux/Imaging/ImageProcessing.hpp>

#include <algorithm>


namespace Halomux::Imaging
{
  namespace ImageProcessing
  {
    void rescale(
      const ImageData& inputImageData,
      ImageData& outputImageData,
      int outputWidth
    )
    {
      int sourceHeight = inputImageData.height();
      int sourceWidth = inputImageData.width();

      if(outputWidth < 1 || sourceWidth < outputWidth){
        outputImageData = inputImageData;
        return;
      }

      float scaleRatio = static_cast<float>(outputWidth) / static_cast<float>(sourceWidth);

      int targetHeight = std::max(1, static_cast<int>(static_cast<float>(sourceHeight) * scaleRatio));

      cv::resize(inputImageData.imageMatrix, outputImageData.imageMatrix, cv::Size(outputWidth, targetHeight), 0, 0, cv::INTER_AREA);
      outputImageData.format = inputImageData.format;
    }


    void rescaleSampled(
      const ImageData& inputImageData,
      ImageData& outputImageData,
      int outputWidth,
      int maxSampledRowsPerOutputRow
    )
    {
      const int sourceWidth = inputImageData.width();
      const int sourceHeight = inputImageData.height();

      if(outputWidth < 1 || sourceWidth <= outputWidth || maxSampledRowsPerOutputRow < 1){
        rescale(inputImageData, outputImageData, outputWidth);
        return;
      }

      const int targetHeight = std::max(1, static_cast<int>(static_cast<float>(sourceHeight) * static_cast<float>(outputWidth) / static_cast<float>(sourceWidth)));
      const int rowsPerOutputRow = sourceHeight / targetHeight;
      const int rowStride = std::max(1, rowsPerOutputRow / maxSampledRowsPerOutputRow);

      if(rowStride == 1){
        rescale(inputImageData, outputImageData, outputWidth);
        return;
      }

      // View on every rowStride-th row of the input (no copy)
      const cv::Mat& source = inputImageData.imageMatrix;
      const cv::Mat sampledRows(sourceHeight / rowStride, sourceWidth, source.type(), source.data, source.step[0] * static_cast<size_t>(rowStride));

      // Target size computed from the full image: the sampled view is less tall
      cv::resize(sampledRows, outputImageData.imageMatrix, cv::Size(outputWidth, targetHeight), 0, 0, cv::INTER_AREA);
      outputImageData.format = inputImageData.format;
    }


    void toBgr(
      const ImageData& inputImageData,
      ImageData& outputImageData
    )
    {
      switch(inputImageData.format){
        case PixelFormat::RGBA:
          cv::cvtColor(inputImageData.imageMatrix, outputImageData.imageMatrix, cv::COLOR_RGBA2BGR);
          break;

        case PixelFormat::BGRA:
          cv::cvtColor(inputImageData.imageMatrix, outputImageData.imageMatrix, cv::COLOR_BGRA2BGR);
          break;

        case PixelFormat::RGB:
          cv::cvtColor(inputImageData.imageMatrix, outputImageData.imageMatrix, cv::COLOR_RGB2BGR);
          break;

        case PixelFormat::BGR:
          outputImageData.imageMatrix = inputImageData.imageMatrix;
          break;
      }

      outputImageData.format = PixelFormat::BGR;
    }


    cv::Rect subImageRect(
      int width,
      int height,
      const UVs& uvs
    )
    {
      glm::ivec2 a{uvs.min.x * static_cast<float>(width), uvs.min.y * static_cast<float>(height)};
      glm::ivec2 b{uvs.max.x * static_cast<float>(width), uvs.max.y * static_cast<float>(height)};

      const int x0 = std::clamp(a.x, 0, width);
      const int y0 = std::clamp(a.y, 0, height);
      const int x1 = std::clamp(b.x, x0, width);
      const int y1 = std::clamp(b.y, y0, height);

      return {x0, y0, x1 - x0, y1 - y0};
    }


    void getSubImage(
      const ImageData& inputImageData,
      ImageData& outputImageData,
      const UVs& uvs
    )
    {
      // View on the input data (no copy): the output must not outlive the input's buffer
      outputImageData.imageMatrix = inputImageData.imageMatrix(subImageRect(inputImageData.width(), inputImageData.height(), uvs));
      outputImageData.format = inputImageData.format;
    }


    std::vector<Color> averageRegions(
      const ImageData& bgrImageData,
      const std::vector<UVs>& regions
    )
    {
      std::vector<Color> colors;
      colors.reserve(regions.size());

      for(const auto& uvs : regions){
        ImageData crop;
        getSubImage(bgrImageData, crop, uvs);
        colors.push_back(getDominantColor(crop));
      }

      return colors;
    }


    Color getDominantColor(
      const ImageData& inputImageData
    )
    {
      if(inputImageData.width() < 1 || inputImageData.height() < 1){
        return {Color(0, 0, 0)};
      }

      return Algorithms::mean(inputImageData);
    }


    namespace Algorithms
    {
      Color mean(
        const ImageData& imageData
      )
      {
        // Expects a BGR image (see toBgr)
        auto mean = cv::mean(imageData.imageMatrix);

        return Color{
          static_cast<uint8_t>(mean[2]),
          static_cast<uint8_t>(mean[1]),
          static_cast<uint8_t>(mean[0])
        };
      }
    }
  }
}
