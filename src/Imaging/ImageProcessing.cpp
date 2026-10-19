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
      // Clamped before the cast: a region far outside would be cast out of range
      const auto pixel = [](float uv, int size){
        return static_cast<int>(std::clamp(uv, 0.f, 1.f) * static_cast<float>(size));
      };

      // At least one pixel: a region narrower than one, such as one kept from a finer
      // subsample, would average nothing and leave its lamp dark
      const auto span = [&pixel](float from, float to, int size){
        const int last = std::max(size - 1, 0);
        const int begin = std::min(pixel(from, size), last);
        const int end = std::clamp(pixel(to, size), begin + 1, std::max(size, begin + 1));
        return std::pair{begin, size > 0 ? end - begin : 0};
      };

      const auto [x, w] = span(uvs.min.x, uvs.max.x, width);
      const auto [y, h] = span(uvs.min.y, uvs.max.y, height);

      return {x, y, w, h};
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
