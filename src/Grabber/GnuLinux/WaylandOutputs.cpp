#include <Halomux/Grabber/GnuLinux/WaylandOutputs.hpp>

#include <algorithm>
#include <cstring>
#include <memory>
#include <string>

#include <wayland-client.h>
#include <xdg-output-unstable-v1-client-protocol.h>

#include <Halomux/Core/Logger.hpp>


namespace Halomux::Grabber
{
  namespace
  {
    struct Output
    {
      wl_output* output{nullptr};
      uint32_t version{0};
      zxdg_output_v1* xdgOutput{nullptr};
      WaylandOutput info;
      // Fallbacks for compositors without wl_output version 4
      std::string xdgName;
      std::string xdgDescription;
    };


    struct State
    {
      zxdg_output_manager_v1* manager{nullptr};
      std::vector<std::unique_ptr<Output>> outputs;
    };


    // wl_output: only the name and the description are used, the geometry comes from xdg-output
    void onGeometry(void*, wl_output*, int32_t, int32_t, int32_t, int32_t, int32_t, const char*, const char*, int32_t){}
    void onMode(void*, wl_output*, uint32_t, int32_t, int32_t, int32_t){}
    void onOutputDone(void*, wl_output*){}
    void onScale(void*, wl_output*, int32_t){}

    void onName(void* data, wl_output*, const char* name)
    {
      static_cast<Output*>(data)->info.name = name;
    }

    void onDescription(void* data, wl_output*, const char* description)
    {
      static_cast<Output*>(data)->info.description = description;
    }

    const wl_output_listener OutputListener = {
      .geometry = onGeometry,
      .mode = onMode,
      .done = onOutputDone,
      .scale = onScale,
      .name = onName,
      .description = onDescription
    };


    void onLogicalPosition(void* data, zxdg_output_v1*, int32_t x, int32_t y)
    {
      auto& geometry = static_cast<Output*>(data)->info.geometry;
      geometry.x = x;
      geometry.y = y;
    }

    void onLogicalSize(void* data, zxdg_output_v1*, int32_t width, int32_t height)
    {
      auto& geometry = static_cast<Output*>(data)->info.geometry;
      geometry.width = width;
      geometry.height = height;
    }

    void onXdgDone(void*, zxdg_output_v1*){}

    void onXdgName(void* data, zxdg_output_v1*, const char* name)
    {
      static_cast<Output*>(data)->xdgName = name;
    }

    void onXdgDescription(void* data, zxdg_output_v1*, const char* description)
    {
      static_cast<Output*>(data)->xdgDescription = description;
    }

    const zxdg_output_v1_listener XdgOutputListener = {
      .logical_position = onLogicalPosition,
      .logical_size = onLogicalSize,
      .done = onXdgDone,
      .name = onXdgName,
      .description = onXdgDescription
    };


    void onGlobal(void* data, wl_registry* registry, uint32_t id, const char* interface, uint32_t version)
    {
      State* state = static_cast<State*>(data);

      if(std::strcmp(interface, wl_output_interface.name) == 0){
        auto output = std::make_unique<Output>();
        output->version = std::min(version, 4u);
        output->output = static_cast<wl_output*>(wl_registry_bind(registry, id, &wl_output_interface, output->version));
        wl_output_add_listener(output->output, &OutputListener, output.get());
        state->outputs.push_back(std::move(output));
      }
      else if(std::strcmp(interface, zxdg_output_manager_v1_interface.name) == 0){
        state->manager = static_cast<zxdg_output_manager_v1*>(wl_registry_bind(registry, id, &zxdg_output_manager_v1_interface, std::min(version, 3u)));
      }
    }

    void onGlobalRemove(void*, wl_registry*, uint32_t){}

    const wl_registry_listener RegistryListener = {
      .global = onGlobal,
      .global_remove = onGlobalRemove
    };
  }


  std::vector<WaylandOutput> listWaylandOutputs()
  {
    std::vector<WaylandOutput> outputs;

    wl_display* display = wl_display_connect(nullptr);
    if(!display){
      Core::Logger::warn("Could not connect to the Wayland compositor: monitors are named by number");
      return outputs;
    }

    State state;
    wl_registry* registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &RegistryListener, &state);

    // Announces the globals, then the outputs' own events
    wl_display_roundtrip(display);

    if(state.manager){
      for(auto& output : state.outputs){
        output->xdgOutput = zxdg_output_manager_v1_get_xdg_output(state.manager, output->output);
        zxdg_output_v1_add_listener(output->xdgOutput, &XdgOutputListener, output.get());
      }

      wl_display_roundtrip(display);

      for(const auto& output : state.outputs){
        WaylandOutput info = output->info;
        if(info.name.empty()){
          info.name = output->xdgName;
        }
        if(info.description.empty()){
          info.description = output->xdgDescription;
        }
        outputs.push_back(std::move(info));
      }
    }
    else{
      Core::Logger::warn("The Wayland compositor has no xdg-output: monitors are named by number");
    }

    for(auto& output : state.outputs){
      if(output->xdgOutput){
        zxdg_output_v1_destroy(output->xdgOutput);
      }

      if(output->version >= WL_OUTPUT_RELEASE_SINCE_VERSION){
        wl_output_release(output->output);
      }
      else{
        wl_output_destroy(output->output);
      }
    }

    if(state.manager){
      zxdg_output_manager_v1_destroy(state.manager);
    }

    wl_registry_destroy(registry);
    wl_display_disconnect(display);

    return outputs;
  }
}
