#include <array>

#include <Halomux/Core/Logger.hpp>
#include <Halomux/Grabber/GnuLinux/Pipewire/DmaBufPlanes.hpp>

using namespace Halomux;


namespace
{
  int failures = 0;

  void expect(const char* name, bool condition)
  {
    if(!condition){
      Core::Logger::error(name);
      failures++;
    }
  }


  // A Pipewire buffer as the compositor fills it, with one data per plane
  struct Buffer
  {
    std::array<spa_chunk, 5> chunks{};
    std::array<spa_data, 5> datas{};
    spa_buffer buffer{};

    explicit Buffer(uint32_t planes)
    {
      for(uint32_t i = 0; i < datas.size(); i++){
        chunks[i].offset = 100 * i;
        chunks[i].stride = static_cast<int32_t>(1000 + i);
        chunks[i].size = 4096;
        datas[i].type = SPA_DATA_DmaBuf;
        datas[i].fd = static_cast<int64_t>(10 + i);
        datas[i].chunk = &chunks[i];
      }

      buffer.n_datas = planes;
      buffer.datas = datas.data();
    }
  };
}


/**
 * @brief Checks the reading of DMA-BUF planes from Pipewire buffers
 *
 */
int main()
{
  {
    Buffer single(1);
    const auto planes = Grabber::dmaBufPlanes(single.buffer);
    expect("Single plane", planes && planes->size() == 1 && planes->at(0).fd == 10 && planes->at(0).offset == 0 && planes->at(0).stride == 1000);
  }

  {
    // Such as Intel CCS: the image, then the compression metadata
    Buffer compressed(2);
    const auto planes = Grabber::dmaBufPlanes(compressed.buffer);
    expect("Every plane read", planes && planes->size() == 2);
    expect("Second plane's own fd, offset and stride", planes && planes->size() == 2 && planes->at(1).fd == 11 && planes->at(1).offset == 100 && planes->at(1).stride == 1001);
  }

  {
    Buffer tooMany(5);
    expect("More planes than EGL imports refused", !Grabber::dmaBufPlanes(tooMany.buffer).has_value());

    Buffer none(0);
    expect("No plane refused", !Grabber::dmaBufPlanes(none.buffer).has_value());

    Buffer noChunk(2);
    noChunk.datas[1].chunk = nullptr;
    expect("Plane without chunk refused", !Grabber::dmaBufPlanes(noChunk.buffer).has_value());

    Buffer noFd(2);
    noFd.datas[1].fd = -1;
    expect("Plane without fd refused", !Grabber::dmaBufPlanes(noFd.buffer).has_value());
  }

  if(failures > 0){
    Core::Logger::error(failures, " check(s) failed");
    return 1;
  }

  Core::Logger::log("DMA-BUF planes: OK");
  return 0;
}
