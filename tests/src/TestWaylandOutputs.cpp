#include <Huenicorn/Core/Logger.hpp>
#include <Huenicorn/Grabber/GnuLinux/WaylandOutputs.hpp>

using namespace Huenicorn;


/**
 * @brief Prints the monitors of the Wayland compositor, to compare with the desktop's display settings
 *
 * Usage: TestWaylandOutputs
 */
int main()
{
  const auto outputs = Grabber::listWaylandOutputs();
  for(const auto& output : outputs){
    const auto& geometry = output.geometry;
    Core::Logger::log(output.name, " \"", output.description, "\" ", geometry.x, ",", geometry.y, " ", geometry.width, "x", geometry.height);
  }

  return outputs.empty() ? 1 : 0;
}
