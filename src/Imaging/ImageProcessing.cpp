#include <Huenicorn/Imaging/ImageProcessing.hpp>

#include <algorithm>


namespace Huenicorn::Imaging
{
  namespace ImageProcessing
  {
    namespace
    {
      cv::InterpolationFlags _toCvInterpolation(
        Interpolation::Type interpolation
      )
      {
        switch(interpolation){
          case Interpolation::Type::Nearest:
            return cv::InterpolationFlags::INTER_NEAREST;

          case Interpolation::Type::Cubic:
            return cv::InterpolationFlags::INTER_CUBIC;

          case Interpolation::Type::Area:
            return cv::InterpolationFlags::INTER_AREA;
        }

        return cv::InterpolationFlags::INTER_AREA;
      }
    }


    void rescale(
      const ImageData& inputImageData,
      ImageData& outputImageData,
      int outputWidth,
      Interpolation::Type interpolation
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

      cv::resize(inputImageData.imageMatrix, outputImageData.imageMatrix, cv::Size(outputWidth, targetHeight), 0, 0, _toCvInterpolation(interpolation));
      outputImageData.format = inputImageData.format;
    }


    void rescaleSampled(
      const ImageData& inputImageData,
      ImageData& outputImageData,
      int outputWidth,
      Interpolation::Type interpolation,
      int maxSampledRowsPerOutputRow
    )
    {
      const int sourceWidth = inputImageData.width();
      const int sourceHeight = inputImageData.height();

      if(outputWidth < 1 || sourceWidth <= outputWidth || maxSampledRowsPerOutputRow < 1){
        rescale(inputImageData, outputImageData, outputWidth, interpolation);
        return;
      }

      const int targetHeight = std::max(1, static_cast<int>(static_cast<float>(sourceHeight) * static_cast<float>(outputWidth) / static_cast<float>(sourceWidth)));
      const int rowsPerOutputRow = sourceHeight / targetHeight;
      const int rowStride = std::max(1, rowsPerOutputRow / maxSampledRowsPerOutputRow);

      if(rowStride == 1){
        rescale(inputImageData, outputImageData, outputWidth, interpolation);
        return;
      }

      // View on every rowStride-th row of the input (no copy)
      const cv::Mat& source = inputImageData.imageMatrix;
      const cv::Mat sampledRows(sourceHeight / rowStride, sourceWidth, source.type(), source.data, source.step[0] * static_cast<size_t>(rowStride));

      // Target size computed from the full image: the sampled view is less tall
      cv::resize(sampledRows, outputImageData.imageMatrix, cv::Size(outputWidth, targetHeight), 0, 0, _toCvInterpolation(interpolation));
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


    void getSubImage(
      const ImageData& inputImageData,
      ImageData& outputImageData,
      const UVs& uvs
    )
    {
      float width = static_cast<float>(inputImageData.width());
      float height = static_cast<float>(inputImageData.height());
      glm::ivec2 a{uvs.min.x * width, uvs.min.y * height};
      glm::ivec2 b{uvs.max.x * width, uvs.max.y * height};
      
      cv::Range cols(std::max(0, a.x), std::min(b.x, inputImageData.width()));
      cv::Range rows(std::max(0, a.y), std::min(b.y, inputImageData.height()));

      // View on the input data (no copy): the output must not outlive the input's buffer
      outputImageData.imageMatrix = inputImageData.imageMatrix(rows, cols);
      outputImageData.format = inputImageData.format;
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
