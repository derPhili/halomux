#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <GLES2/gl2ext.h>

#include <opencv2/core/mat.hpp>
#include <opencv2/core/types.hpp>


namespace Huenicorn::Grabber
{
  /**
   * @brief Downscales DMA-BUF screen frames on the GPU
   *
   * Frames are imported without copy as EGL images and reduced by a fragment shader computing
   * the exact average of each block of source pixels. Only the small result is read back.
   * Every method must be called from the thread that called init().
   */
  class EglDownscaler
  {
  public:
    /**
     * @brief Single plane DMA-BUF description
     *
     */
    struct DmaBuf
    {
      int fd;
      uint32_t offset;
      uint32_t stride;
      int width;
      int height;
      uint32_t drmFormat;
      uint64_t modifier;
    };


    /**
     * @brief Imported DMA-BUF, reusable as long as the underlying buffer lives
     *
     */
    struct Image
    {
      EGLImage eglImage{EGL_NO_IMAGE};
      GLuint texture{0};
      int width{0};
      int height{0};
    };


    EglDownscaler() = default;
    ~EglDownscaler();

    EglDownscaler(const EglDownscaler&) = delete;
    EglDownscaler& operator=(const EglDownscaler&) = delete;


    /**
     * @brief Creates a surfaceless GLES 3 context on the GPU and makes it current
     *
     * @return true The GPU path is usable
     * @return false A required EGL/GLES feature is missing
     */
    bool init();


    /**
     * @brief Returns the DRM format modifiers the GPU can import for a DRM format
     *
     * @param drmFormat DRM fourcc code
     * @return std::vector<uint64_t> Importable modifiers, empty if none
     */
    std::vector<uint64_t> supportedModifiers(
      uint32_t drmFormat
    ) const;


    /**
     * @brief Returns the name of the GPU (GL renderer string)
     *
     * @return const std::string& GPU name
     */
    const std::string& renderer() const
    {
      return m_renderer;
    }


    /**
     * @brief Imports a DMA-BUF
     *
     * @param dmaBuf Buffer to import
     * @return std::unique_ptr<Image> Imported image, null on failure
     */
    std::unique_ptr<Image> importDmaBuf(
      const DmaBuf& dmaBuf
    );


    /**
     * @brief Releases an imported image
     *
     * @param image Image to release
     */
    void releaseImage(
      Image& image
    );


    /**
     * @brief Averages the image down to outputWidth x outputHeight
     *
     * @param image Imported source image
     * @param outputWidth Output width
     * @param outputHeight Output height
     * @param output RGBA output (CV_8UC4), top row first
     * @return true Downscale succeeded
     * @return false A GL error occurred
     */
    bool downscale(
      const Image& image,
      int outputWidth,
      int outputHeight,
      cv::Mat& output
    );


    /**
     * @brief Maximal number of regions averaged at once (an entertainment area has up to 20 channels)
     *
     */
    static constexpr int MaxRegions = 32;


    /**
     * @brief Averages regions of the image, entirely on the GPU
     *
     * The image is first averaged down to subsampleWidth x subsampleHeight like downscale(),
     * then each region of that subsample is averaged into a single pixel. Only one pixel per
     * region is read back.
     *
     * @param image Imported source image
     * @param subsampleWidth Subsample width
     * @param subsampleHeight Subsample height
     * @param regions Regions in subsample pixels (see ImageProcessing::subImageRect), at most MaxRegions
     * @param colors RGBA average per region, transparent black for empty regions
     * @param slot Render targets to use: one per source size, such as one per monitor, so they
     *        aren't reallocated (and waited for) when sources of different sizes alternate
     * @return true Averages computed
     * @return false A GL error occurred
     */
    bool averageRegions(
      const Image& image,
      int subsampleWidth,
      int subsampleHeight,
      const std::vector<cv::Rect>& regions,
      std::vector<cv::Vec4b>& colors,
      size_t slot = 0
    );


  private:
    /**
     * @brief Render targets of one slot, sized on demand
     *
     */
    struct Targets
    {
      GLuint targetTexture{0};
      GLuint framebuffer{0};
      int targetWidth{0};
      int targetHeight{0};

      GLuint rowMeansTexture{0};
      GLuint rowMeansFramebuffer{0};
      int rowMeansRowCount{0};

      GLuint regionsTexture{0};
      GLuint regionsFramebuffer{0};
      int regionsTargetCount{0};
    };


    void _selectSlot(size_t slot);
    bool _initDisplay();
    bool _initContext();
    bool _initProgram();
    bool _initRegionsProgram();
    bool _ensureTarget(int width, int height);
    bool _ensureRegionsTarget(int regionCount, int rowCount);
    bool _renderSubsample(const Image& image, int outputWidth, int outputHeight);
    void _shutdown();


    // EGL
    std::string m_renderer;
    EGLDisplay m_display{EGL_NO_DISPLAY};
    EGLContext m_context{EGL_NO_CONTEXT};

    PFNEGLQUERYDMABUFMODIFIERSEXTPROC m_eglQueryDmaBufModifiers{nullptr};
    PFNGLEGLIMAGETARGETTEXTURE2DOESPROC m_glEGLImageTargetTexture2D{nullptr};

    // GLES
    GLuint m_program{0};
    GLint m_sourceSizeLocation{-1};
    GLint m_blockSizeLocation{-1};
    GLint m_tapsLocation{-1};

    // Render targets per slot, and the slot in use
    std::vector<Targets> m_targets;
    Targets* m_active{nullptr};

    // Region averaging: row means (one per region and subsample row), then one pixel per region
    GLuint m_rowMeansProgram{0};
    GLint m_rowMeansRegionsLocation{-1};

    GLuint m_regionMeansProgram{0};
    GLint m_regionMeansRegionsLocation{-1};
  };
}
