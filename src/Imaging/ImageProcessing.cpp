#include <Huenicorn/Imaging/ImageProcessing.hpp>

#include <algorithm>


namespace Huenicorn::Imaging
{
  namespace ImageProcessing
  {
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

      cv::InterpolationFlags interpolationFlag = cv::InterpolationFlags::INTER_AREA;

      switch(interpolation){
        case Interpolation::Type::Nearest:
          interpolationFlag = cv::InterpolationFlags::INTER_NEAREST;
          break;

        case Interpolation::Type::Cubic:
          interpolationFlag = cv::InterpolationFlags::INTER_CUBIC;
          break;

        case Interpolation::Type::Area:
          interpolationFlag = cv::InterpolationFlags::INTER_AREA;
          break;
      }

      cv::resize(inputImageData.imageMatrix, outputImageData.imageMatrix, cv::Size(outputWidth, targetHeight), 0, 0, interpolationFlag);
    }


    void rgbaToRgb(
      const ImageData& inputImageData,
      ImageData& outputImageData
    )
    {
      cv::cvtColor(inputImageData.imageMatrix, outputImageData.imageMatrix, cv::COLOR_RGBA2RGB);
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

      outputImageData.imageMatrix = inputImageData.imageMatrix(rows, cols).clone();
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
