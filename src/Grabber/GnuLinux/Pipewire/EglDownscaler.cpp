#include <Huenicorn/Grabber/GnuLinux/Pipewire/EglDownscaler.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>

#include <Huenicorn/Core/Logger.hpp>


namespace Huenicorn::Grabber
{
  namespace
  {
    // Fullscreen triangle, no vertex buffer needed
    constexpr const char* VertexShaderSource = R"(#version 300 es
void main()
{
  vec2 position = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));
  gl_Position = vec4(position * 2.0 - 1.0, 0.0, 1.0);
}
)";


    // Each output pixel averages a block of source texels. Taps are placed on the corners shared
    // by 2x2 texels, where bilinear filtering returns their exact average: a 16x16 block takes
    // 8x8 fetches and still accounts for every texel.
    constexpr const char* FragmentShaderSource = R"(#version 300 es
#extension GL_OES_EGL_image_external_essl3 : require
precision highp float;

uniform samplerExternalOES u_source;
uniform vec2 u_sourceSize;
uniform vec2 u_blockSize;
uniform ivec2 u_taps;

out vec4 fragColor;

void main()
{
  // Output row 0 maps to the first source row, so the read back image is top row first
  vec2 blockOrigin = floor(gl_FragCoord.xy) * u_blockSize;
  vec2 tapSpacing = u_blockSize / vec2(u_taps);

  vec4 sum = vec4(0.0);
  for(int y = 0; y < u_taps.y; y++){
    for(int x = 0; x < u_taps.x; x++){
      vec2 texel = blockOrigin + (vec2(x, y) + 0.5) * tapSpacing;
      sum += texture(u_source, texel / u_sourceSize);
    }
  }

  fragColor = sum / float(u_taps.x * u_taps.y);
}
)";


    bool hasExtension(
      const char* extensions,
      const char* name
    )
    {
      if(!extensions){
        return false;
      }

      const size_t length = std::strlen(name);
      for(const char* position = extensions; (position = std::strstr(position, name)) != nullptr; position += length){
        const bool startOk = position == extensions || position[-1] == ' ';
        const bool endOk = position[length] == ' ' || position[length] == '\0';
        if(startOk && endOk){
          return true;
        }
      }

      return false;
    }


    GLuint compileShader(
      GLenum type,
      const char* source
    )
    {
      GLuint shader = glCreateShader(type);
      glShaderSource(shader, 1, &source, nullptr);
      glCompileShader(shader);

      GLint compiled = GL_FALSE;
      glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
      if(compiled != GL_TRUE){
        char log[1024] = {};
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        Core::Logger::error("[gpu] Shader compilation failed: ", log);
        glDeleteShader(shader);
        return 0;
      }

      return shader;
    }
  }


  EglDownscaler::~EglDownscaler()
  {
    _shutdown();
  }


  bool EglDownscaler::init()
  {
    if(!_initDisplay() || !_initContext() || !_initProgram()){
      _shutdown();
      return false;
    }

    return true;
  }


  std::vector<uint64_t> EglDownscaler::supportedModifiers(
    uint32_t drmFormat
  ) const
  {
    EGLint count = 0;
    if(!m_eglQueryDmaBufModifiers(m_display, static_cast<EGLint>(drmFormat), 0, nullptr, nullptr, &count) || count <= 0){
      return {};
    }

    std::vector<EGLuint64KHR> modifiers(static_cast<size_t>(count));
    std::vector<EGLBoolean> externalOnly(static_cast<size_t>(count));
    if(!m_eglQueryDmaBufModifiers(m_display, static_cast<EGLint>(drmFormat), count, modifiers.data(), externalOnly.data(), &count)){
      return {};
    }

    // External-only modifiers are fine: frames are sampled through samplerExternalOES
    modifiers.resize(static_cast<size_t>(count));
    return {modifiers.begin(), modifiers.end()};
  }


  std::unique_ptr<EglDownscaler::Image> EglDownscaler::importDmaBuf(
    const DmaBuf& dmaBuf
  )
  {
    const EGLAttrib attributes[] = {
      EGL_WIDTH, dmaBuf.width,
      EGL_HEIGHT, dmaBuf.height,
      EGL_LINUX_DRM_FOURCC_EXT, static_cast<EGLAttrib>(dmaBuf.drmFormat),
      EGL_DMA_BUF_PLANE0_FD_EXT, dmaBuf.fd,
      EGL_DMA_BUF_PLANE0_OFFSET_EXT, static_cast<EGLAttrib>(dmaBuf.offset),
      EGL_DMA_BUF_PLANE0_PITCH_EXT, static_cast<EGLAttrib>(dmaBuf.stride),
      EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT, static_cast<EGLAttrib>(dmaBuf.modifier & 0xffffffff),
      EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT, static_cast<EGLAttrib>(dmaBuf.modifier >> 32),
      EGL_NONE
    };

    EGLImage eglImage = eglCreateImage(m_display, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, nullptr, attributes);
    if(eglImage == EGL_NO_IMAGE){
      Core::Logger::error("[gpu] Failed to import DMA-BUF (EGL error 0x", std::hex, eglGetError(), std::dec, ")");
      return nullptr;
    }

    auto image = std::make_unique<Image>();
    image->eglImage = eglImage;
    image->width = dmaBuf.width;
    image->height = dmaBuf.height;

    glGenTextures(1, &image->texture);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, image->texture);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    m_glEGLImageTargetTexture2D(GL_TEXTURE_EXTERNAL_OES, static_cast<GLeglImageOES>(eglImage));

    if(GLenum error = glGetError(); error != GL_NO_ERROR){
      Core::Logger::error("[gpu] Failed to bind DMA-BUF to a texture (GL error 0x", std::hex, error, std::dec, ")");
      releaseImage(*image);
      return nullptr;
    }

    return image;
  }


  void EglDownscaler::releaseImage(
    Image& image
  )
  {
    if(image.texture != 0){
      glDeleteTextures(1, &image.texture);
      image.texture = 0;
    }

    if(image.eglImage != EGL_NO_IMAGE){
      eglDestroyImage(m_display, image.eglImage);
      image.eglImage = EGL_NO_IMAGE;
    }
  }


  bool EglDownscaler::downscale(
    const Image& image,
    int outputWidth,
    int outputHeight,
    cv::Mat& output
  )
  {
    if(!_ensureTarget(outputWidth, outputHeight)){
      return false;
    }

    const float blockWidth = static_cast<float>(image.width) / static_cast<float>(outputWidth);
    const float blockHeight = static_cast<float>(image.height) / static_cast<float>(outputHeight);

    // One tap per 2x2 texels, at least one per block
    const int tapsX = std::max(1, static_cast<int>(std::ceil(blockWidth / 2.f)));
    const int tapsY = std::max(1, static_cast<int>(std::ceil(blockHeight / 2.f)));

    glBindFramebuffer(GL_FRAMEBUFFER, m_framebuffer);
    glViewport(0, 0, outputWidth, outputHeight);

    glUseProgram(m_program);
    glUniform2f(m_sourceSizeLocation, static_cast<float>(image.width), static_cast<float>(image.height));
    glUniform2f(m_blockSizeLocation, blockWidth, blockHeight);
    glUniform2i(m_tapsLocation, tapsX, tapsY);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, image.texture);

    glDrawArrays(GL_TRIANGLES, 0, 3);

    // Synchronous: once it returns, the GPU is done with the source buffer
    output.create(outputHeight, outputWidth, CV_8UC4);
    glPixelStorei(GL_PACK_ALIGNMENT, 4);
    glReadPixels(0, 0, outputWidth, outputHeight, GL_RGBA, GL_UNSIGNED_BYTE, output.data);

    if(GLenum error = glGetError(); error != GL_NO_ERROR){
      Core::Logger::error("[gpu] Downscale failed (GL error 0x", std::hex, error, std::dec, ")");
      return false;
    }

    return true;
  }


  bool EglDownscaler::_initDisplay()
  {
    const char* clientExtensions = eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);
    if(!hasExtension(clientExtensions, "EGL_EXT_platform_device")){
      Core::Logger::warn("[gpu] EGL_EXT_platform_device is not available");
      return false;
    }

    auto eglQueryDevices = reinterpret_cast<PFNEGLQUERYDEVICESEXTPROC>(eglGetProcAddress("eglQueryDevicesEXT"));
    auto eglQueryDeviceString = reinterpret_cast<PFNEGLQUERYDEVICESTRINGEXTPROC>(eglGetProcAddress("eglQueryDeviceStringEXT"));
    if(!eglQueryDevices || !eglQueryDeviceString){
      return false;
    }

    EGLDeviceEXT devices[8];
    EGLint deviceCount = 0;
    if(!eglQueryDevices(8, devices, &deviceCount) || deviceCount == 0){
      Core::Logger::warn("[gpu] No EGL device found");
      return false;
    }

    // Pick the first hardware device (one exposing a DRM render node), skipping software renderers
    for(EGLint i = 0; i < deviceCount && m_display == EGL_NO_DISPLAY; i++){
      const char* renderNode = eglQueryDeviceString(devices[i], EGL_DRM_RENDER_NODE_FILE_EXT);
      if(!renderNode){
        continue;
      }

      EGLDisplay display = eglGetPlatformDisplay(EGL_PLATFORM_DEVICE_EXT, devices[i], nullptr);
      if(display == EGL_NO_DISPLAY || !eglInitialize(display, nullptr, nullptr)){
        continue;
      }

      m_display = display;
      Core::Logger::log("[gpu] Using EGL device ", renderNode, " (", eglQueryString(display, EGL_VENDOR), ")");
    }

    if(m_display == EGL_NO_DISPLAY){
      Core::Logger::warn("[gpu] No usable EGL hardware device");
      return false;
    }

    const char* displayExtensions = eglQueryString(m_display, EGL_EXTENSIONS);
    for(const char* required : {"EGL_EXT_image_dma_buf_import", "EGL_EXT_image_dma_buf_import_modifiers", "EGL_KHR_surfaceless_context"}){
      if(!hasExtension(displayExtensions, required)){
        Core::Logger::warn("[gpu] Missing EGL extension ", required);
        return false;
      }
    }

    m_eglQueryDmaBufModifiers = reinterpret_cast<PFNEGLQUERYDMABUFMODIFIERSEXTPROC>(eglGetProcAddress("eglQueryDmaBufModifiersEXT"));
    return m_eglQueryDmaBufModifiers != nullptr;
  }


  bool EglDownscaler::_initContext()
  {
    if(!eglBindAPI(EGL_OPENGL_ES_API)){
      return false;
    }

    const EGLint configAttributes[] = {
      EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
      EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
      EGL_NONE
    };

    EGLConfig config;
    EGLint configCount = 0;
    if(!eglChooseConfig(m_display, configAttributes, &config, 1, &configCount) || configCount == 0){
      Core::Logger::warn("[gpu] No GLES 3 EGL config");
      return false;
    }

    const EGLint contextAttributes[] = {
      EGL_CONTEXT_MAJOR_VERSION, 3,
      EGL_NONE
    };

    m_context = eglCreateContext(m_display, config, EGL_NO_CONTEXT, contextAttributes);
    if(m_context == EGL_NO_CONTEXT){
      Core::Logger::warn("[gpu] Failed to create a GLES 3 context");
      return false;
    }

    // Surfaceless: rendering only targets our own framebuffer
    if(!eglMakeCurrent(m_display, EGL_NO_SURFACE, EGL_NO_SURFACE, m_context)){
      Core::Logger::warn("[gpu] Failed to make the GLES context current");
      return false;
    }

    const char* glExtensions = reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS));
    if(!hasExtension(glExtensions, "GL_OES_EGL_image_external_essl3")){
      Core::Logger::warn("[gpu] Missing GL_OES_EGL_image_external_essl3");
      return false;
    }

    m_glEGLImageTargetTexture2D = reinterpret_cast<PFNGLEGLIMAGETARGETTEXTURE2DOESPROC>(eglGetProcAddress("glEGLImageTargetTexture2DOES"));
    return m_glEGLImageTargetTexture2D != nullptr;
  }


  bool EglDownscaler::_initProgram()
  {
    GLuint vertexShader = compileShader(GL_VERTEX_SHADER, VertexShaderSource);
    GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, FragmentShaderSource);

    if(vertexShader == 0 || fragmentShader == 0){
      glDeleteShader(vertexShader);
      glDeleteShader(fragmentShader);
      return false;
    }

    m_program = glCreateProgram();
    glAttachShader(m_program, vertexShader);
    glAttachShader(m_program, fragmentShader);
    glLinkProgram(m_program);
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    GLint linked = GL_FALSE;
    glGetProgramiv(m_program, GL_LINK_STATUS, &linked);
    if(linked != GL_TRUE){
      char log[1024] = {};
      glGetProgramInfoLog(m_program, sizeof(log), nullptr, log);
      Core::Logger::error("[gpu] Shader program link failed: ", log);
      return false;
    }

    glUseProgram(m_program);
    glUniform1i(glGetUniformLocation(m_program, "u_source"), 0);
    m_sourceSizeLocation = glGetUniformLocation(m_program, "u_sourceSize");
    m_blockSizeLocation = glGetUniformLocation(m_program, "u_blockSize");
    m_tapsLocation = glGetUniformLocation(m_program, "u_taps");

    glGenFramebuffers(1, &m_framebuffer);

    return glGetError() == GL_NO_ERROR;
  }


  bool EglDownscaler::_ensureTarget(
    int width,
    int height
  )
  {
    if(m_targetTexture != 0 && m_targetWidth == width && m_targetHeight == height){
      return true;
    }

    if(m_targetTexture != 0){
      glDeleteTextures(1, &m_targetTexture);
      m_targetTexture = 0;
    }

    glGenTextures(1, &m_targetTexture);
    glBindTexture(GL_TEXTURE_2D, m_targetTexture);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, width, height);

    glBindFramebuffer(GL_FRAMEBUFFER, m_framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_targetTexture, 0);

    if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE){
      Core::Logger::error("[gpu] Incomplete framebuffer for a ", width, "x", height, " target");
      return false;
    }

    m_targetWidth = width;
    m_targetHeight = height;
    return true;
  }


  void EglDownscaler::_shutdown()
  {
    if(m_display == EGL_NO_DISPLAY){
      return;
    }

    if(m_context != EGL_NO_CONTEXT){
      eglMakeCurrent(m_display, EGL_NO_SURFACE, EGL_NO_SURFACE, m_context);

      if(m_targetTexture != 0){
        glDeleteTextures(1, &m_targetTexture);
      }

      if(m_framebuffer != 0){
        glDeleteFramebuffers(1, &m_framebuffer);
      }

      if(m_program != 0){
        glDeleteProgram(m_program);
      }

      eglMakeCurrent(m_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
      eglDestroyContext(m_display, m_context);
      m_context = EGL_NO_CONTEXT;
    }

    eglTerminate(m_display);
    m_display = EGL_NO_DISPLAY;
    m_targetTexture = 0;
    m_framebuffer = 0;
    m_program = 0;
  }
}
