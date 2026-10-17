#pragma once

#include <opencv2/opencv.hpp>

namespace Halomux::Imaging
{
  enum class PixelFormat {
    RGB,
    RGBA,
    BGR,
    BGRA
  };


  struct ImageData
  {
    cv::Mat imageMatrix;
    PixelFormat format{PixelFormat::BGR};

    inline int width() const
    {
      return imageMatrix.cols;
    }

    inline int height() const
    {
      return imageMatrix.rows;
    }

    inline bool hasData() const
    {
      return imageMatrix.data != nullptr && width() > 0 && height() > 0;
    }
  };
}
