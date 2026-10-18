#include <Halomux/Grabber/GnuLinux/Pipewire/DmaBufPlanes.hpp>


namespace Halomux::Grabber
{
  std::optional<std::vector<DmaBufPlane>> dmaBufPlanes(
    const spa_buffer& buffer
  )
  {
    if(buffer.n_datas == 0 || buffer.n_datas > MaxDmaBufPlanes || buffer.datas == nullptr){
      return std::nullopt;
    }

    std::vector<DmaBufPlane> planes;
    for(uint32_t i = 0; i < buffer.n_datas; i++){
      const spa_data& data = buffer.datas[i];
      if(data.chunk == nullptr || data.fd < 0){
        return std::nullopt;
      }

      planes.push_back({
        .fd = static_cast<int>(data.fd),
        .offset = data.chunk->offset,
        .stride = static_cast<uint32_t>(data.chunk->stride)
      });
    }

    return planes;
  }
}
