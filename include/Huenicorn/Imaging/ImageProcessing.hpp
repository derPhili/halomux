#pragma once

#include <vector>

#include <Huenicorn/Imaging/ImageData.hpp>
#include <Huenicorn/Imaging/Interpolation.hpp>
#include <Huenicorn/Imaging/Color.hpp>
#include <Huenicorn/Imaging/UV.hpp>


namespace Huenicorn::Imaging
{
  using Colors = std::vector<Color>;

  /**
   * @brief Provides image manipulation functions
   * 
   */
  namespace ImageProcessing
  {
    /**
     * @brief Outputs a resampled bitmap of an input bitmap
     * 
     * @param image Input bitmap
     * @param targetWidth Target width of the output bitmap
     * @param interpolationType Subsampling interpolation type
     */
    void rescale(
      const ImageData& inputImageData,
      ImageData& outputImageData,
      int outputWidth,
      Interpolation::Type interpolationType
    );


    /**
     * @brief Converts an image to the BGR layout expected by the color computation
     *
     * @param inputImageData Input image, in any supported pixel format
     * @param outputImageData Output BGR image (may be the input itself)
     */
    void toBgr(
      const ImageData& inputImageData,
      ImageData& outputImageData
    );


    /**
     * @brief Outputs a rectangular portion of the source Image
     * 
     * @param sourceImage Input image
     * @param a Top-left coordinates
     * @param b Bottom-right coordinates
     */
    void getSubImage(
      const ImageData& sourceImageData,
      ImageData& destImage,
      const UVs& uvs
    );


    /**
     * @brief Get the Dominant Color
     * 
     * @param image Input image
     * @return Color Dominant color
     */
    Color getDominantColor(
      const ImageData& imageData
    );


    namespace Algorithms
    {
      Color mean(
        const ImageData& imageData
      );
    }
  };
}
