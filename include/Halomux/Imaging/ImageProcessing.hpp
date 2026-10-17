#pragma once

#include <vector>

#include <Halomux/Imaging/ImageData.hpp>
#include <Halomux/Imaging/Color.hpp>
#include <Halomux/Imaging/UV.hpp>


namespace Halomux::Imaging
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
     */
    void rescale(
      const ImageData& inputImageData,
      ImageData& outputImageData,
      int outputWidth
    );


    /**
     * @brief Downscales an image like rescale(), reading only a subset of its rows
     *
     * Only every n-th row is read so that each output row averages at most
     * maxSampledRowsPerOutputRow input rows. The result stays close to a full
     * rescale for the color averages it feeds, at a fraction of the memory reads.
     *
     * @param inputImageData Input image
     * @param outputImageData Output image
     * @param outputWidth Width of the output image
     * @param maxSampledRowsPerOutputRow Number of input rows averaged into each output row
     */
    void rescaleSampled(
      const ImageData& inputImageData,
      ImageData& outputImageData,
      int outputWidth,
      int maxSampledRowsPerOutputRow = 4
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
     * @brief Returns the pixels covered by normalized coordinates in an image
     *
     * Shared by the CPU and GPU paths so both average the very same pixels.
     *
     * @param width Image width
     * @param height Image height
     * @param uvs Normalized region
     * @return cv::Rect Pixel region, clamped to the image (possibly empty)
     */
    cv::Rect subImageRect(
      int width,
      int height,
      const UVs& uvs
    );


    /**
     * @brief Outputs a rectangular portion of the source Image
     * 
     * @param sourceImageData Input image
     * @param destImage View on the region of the input image
     * @param uvs Normalized region
     */
    void getSubImage(
      const ImageData& sourceImageData,
      ImageData& destImage,
      const UVs& uvs
    );


    /**
     * @brief Returns the average color of each region of a BGR image
     *
     * @param bgrImageData BGR image
     * @param regions Normalized regions
     * @return std::vector<Color> Average color per region, black for empty regions
     */
    std::vector<Color> averageRegions(
      const ImageData& bgrImageData,
      const std::vector<UVs>& regions
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
