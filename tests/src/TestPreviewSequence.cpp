#include <Halomux/Core/Logger.hpp>
#include <Halomux/Core/PreviewSequence.hpp>

using namespace Halomux;
using namespace Halomux::Core;


namespace
{
  int failures = 0;

  void expect(const char* name, const Imaging::Color& color, const Imaging::Color& expected)
  {
    if(color != expected){
      Core::Logger::error(name, ": ", int(color.m_r), " ", int(color.m_g), " ", int(color.m_b), " instead of ", int(expected.m_r), " ", int(expected.m_g), " ", int(expected.m_b));
      failures++;
    }
  }
}


/**
 * @brief Checks the preview sequence segments
 *
 */
int main()
{
  const Imaging::Color white{255, 255, 255};
  const Imaging::Color dark{25, 25, 30};
  constexpr float Left = 0.2f;
  constexpr float Right = 0.8f;

  // Hard cuts, identical on both sides
  expect("First cut", PreviewSequence::colorAt(0.5f, Left), {255, 40, 40});
  expect("Second cut", PreviewSequence::colorAt(1.5f, Right), {40, 80, 255});
  expect("Last cut", PreviewSequence::colorAt(3.9f, Left), {255, 170, 40});

  // Flashes: left only, right only, then both
  expect("Dark scene", PreviewSequence::colorAt(4.2f, Left), dark);
  expect("Left flash on the left", PreviewSequence::colorAt(4.55f, Left), white);
  expect("Left flash not on the right", PreviewSequence::colorAt(4.55f, Right), dark);
  expect("Right flash on the right", PreviewSequence::colorAt(5.55f, Right), white);
  expect("Right flash not on the left", PreviewSequence::colorAt(5.55f, Left), dark);
  expect("Both flash, left", PreviewSequence::colorAt(6.55f, Left), white);
  expect("Both flash, right", PreviewSequence::colorAt(6.55f, Right), white);
  expect("Flash is short", PreviewSequence::colorAt(4.5f + PreviewSequence::FlashDuration + 0.01f, Left), dark);

  // Hue rotation starts at red on the far left
  expect("Fade start", PreviewSequence::colorAt(7.5f, 0.f), {255, 0, 0});
  expect("Hue of 120 degrees", PreviewSequence::fromHue(120.f), {0, 255, 0});

  // Black, then white
  expect("Black", PreviewSequence::colorAt(10.8f, Left), {0, 0, 0});
  expect("White", PreviewSequence::colorAt(11.5f, Right), white);

  if(failures > 0){
    Core::Logger::error(failures, " test(s) failed");
    return 1;
  }

  Core::Logger::log("All tests passed");
  return 0;
}
