#include <cmath>

#include <Halomux/Core/ChannelColor.hpp>
#include <Halomux/Core/Logger.hpp>

using namespace Halomux;
using Core::ChannelColor::Settings;


namespace
{
  int failures = 0;

  const Imaging::Color Black{0, 0, 0};
  const Imaging::Color White{255, 255, 255};


  void expectNear(const char* name, float value, float expected, float tolerance = 1e-4f)
  {
    if(!(std::abs(value - expected) <= tolerance)){
      Core::Logger::error(name, ": ", value, " instead of ", expected);
      failures++;
    }
  }


  void expect(const char* name, bool condition)
  {
    if(!condition){
      Core::Logger::error(name);
      failures++;
    }
  }


  Hue::Api::Channel lamp(float gammaFactor = 0.f)
  {
    return Hue::Api::Channel(true, {}, gammaFactor);
  }
}


/**
 * @brief Checks the color streamed for a lamp from the color it should show
 *
 */
int main()
{
  // Light reflected by a colored wall: the channels the wall reflects better are dimmed
  {
    using Core::ChannelColor::compensateWall;
    expect("White wall changes nothing", compensateWall(Imaging::Color(200, 120, 40), White) == Imaging::Color(200, 120, 40));

    const Imaging::Color beige{255, 230, 180};
    const auto onBeige = compensateWall(White, beige);
    expect("White on a beige wall loses red and green against blue", onBeige.m_b == 255 && onBeige.m_r < onBeige.m_g && onBeige.m_g < 255);

    // As light: the lamp's color times the wall's reflection is neutral again
    const auto linear = [](float value){value /= 255.f; return value > 0.04045f ? std::pow((value + 0.055f) / 1.055f, 2.4f) : value / 12.92f;};
    expectNear("Reflected white neutral, red against blue", linear(onBeige.m_r) * linear(beige.m_r), linear(onBeige.m_b) * linear(beige.m_b), 0.01f);
    expectNear("Reflected white neutral, green against blue", linear(onBeige.m_g) * linear(beige.m_g), linear(onBeige.m_b) * linear(beige.m_b), 0.01f);

    expect("Black stays black", compensateWall(Black, beige) == Black);

    // A wall reflecting almost nothing of a color: limited, not dividing by nothing
    const auto onOrange = compensateWall(White, Imaging::Color(255, 128, 0));
    expect("Wall without blue limited", onOrange.m_b == 255 && onOrange.m_r > 0);
  }

  // The wall a lamp shines on shapes what it streams, its own one before the common one
  {
    auto plain = lamp();
    auto onWall = lamp();
    Settings beigeWall;
    beigeWall.wallColor = Imaging::Color(255, 230, 180);
    const auto plainXyb = Core::ChannelColor::step(plain, White, Settings{}, 0.f);
    const auto wallXyb = Core::ChannelColor::step(onWall, White, beigeWall, 0.f);
    expect("Bluer on a beige wall", wallXyb.y < plainXyb.y || wallXyb.x < plainXyb.x);
    expect("Dimmer on a beige wall", wallXyb.z < plainXyb.z);

    auto ownWall = lamp();
    ownWall.wallColor = White;
    const auto ownXyb = Core::ChannelColor::step(ownWall, White, beigeWall, 0.f);
    expectNear("A lamp's own wall first", ownXyb.x, plainXyb.x);
  }

  {
    auto channel = lamp();
    const auto xyb = Core::ChannelColor::step(channel, Black, Settings{}, 0.f);
    expectNear("Black x", xyb.x, Imaging::Color::XYBBlack.x);
    expectNear("Black y", xyb.y, Imaging::Color::XYBBlack.y);
    expectNear("Black is off", xyb.z, 0.f);
  }

  {
    auto channel = lamp();
    const auto xyb = Core::ChannelColor::step(channel, White, Settings{}, 0.f);
    expectNear("White is D65 x", xyb.x, 0.3127f, 1e-3f);
    expectNear("White is D65 y", xyb.y, 0.3290f, 1e-3f);
    expectNear("White is full brightness", xyb.z, 1.f);
  }

  {
    // Smoothing eases toward the new color, without overshooting it
    auto channel = lamp();
    const Settings settings{.transitionSmoothing = 0.5f};
    expectNear("First color isn't eased", Core::ChannelColor::step(channel, Black, settings, 0.f).z, 0.f);
    expectNear("Half way", Core::ChannelColor::step(channel, White, settings, 0.f).z, 0.5f);

    float previous = 0.5f;
    bool monotonic = true;
    for(int i = 0; i < 40; i++){
      const float z = Core::ChannelColor::step(channel, White, settings, 0.f).z;
      monotonic = monotonic && z >= previous && z <= 1.f;
      previous = z;
    }
    expect("Smoothing converges without overshoot", monotonic);
    expectNear("Smoothing reaches the color", previous, 1.f, 1e-3f);
  }

  {
    auto channel = lamp();
    const auto xyb = Core::ChannelColor::step(channel, White, Settings{.brightness = 0.4f}, 0.f);
    expectNear("The dimmer scales the brightness", xyb.z, 0.4f);
  }

  {
    // Gamma factor 0.5: exponent 2^-1
    auto channel = lamp(0.5f);
    const Imaging::Color gray{64, 64, 64};
    const auto xyb = Core::ChannelColor::step(channel, gray, Settings{}, 0.f);
    expectNear("Gamma applies to the brightness", xyb.z, std::sqrt(gray.brightness()));
  }

  {
    // Static colors fade, even without smoothing
    auto channel = lamp();
    const Settings settings{.tickDuration = 0.02f, .syncing = false};
    Core::ChannelColor::step(channel, Black, settings, 0.f);
    expectNear("Static colors fade", Core::ChannelColor::step(channel, White, settings, 0.f).z, 1.f - std::exp(-0.02f / 0.35f));
  }

  {
    // The preview follows its sequence like the screen, even while sync is off
    auto channel = lamp();
    const Settings settings{.syncing = false, .previewing = true};
    Core::ChannelColor::step(channel, Black, settings, 0.f);
    expectNear("Preview isn't faded", Core::ChannelColor::step(channel, White, settings, 0.f).z, 1.f);
  }

  {
    // The highlight boost comes after the smoothing, so it isn't eased away
    auto channel = lamp();
    const Settings settings{.transitionSmoothing = 0.5f, .highlightBoost = 1.f};
    Core::ChannelColor::step(channel, Black, settings, 0.f);
    expectNear("Sudden brightening flashes", Core::ChannelColor::step(channel, White, settings, 0.f).z, 1.f);
  }

  {
    auto channel = lamp();
    const Settings settings{.highlightBoost = 1.f, .syncing = false};
    Core::ChannelColor::step(channel, Black, settings, 0.f);
    expectNear("Static colors don't flash", Core::ChannelColor::step(channel, White, settings, 0.f).z, 1.f - std::exp(-0.02f / 0.35f));
  }

  {
    // Sound: a full trigger on steady gray takes the lamp half way to full with strength 0.5
    auto channel = lamp();
    const Imaging::Color gray{128, 128, 128};
    const Settings settings{.highlightBoost = 0.5f};
    const float base = Core::ChannelColor::step(channel, gray, settings, 0.f).z;
    const float boosted = Core::ChannelColor::step(channel, gray, settings, 1.f).z;
    expectNear("Sound triggers the boost", boosted, base + 0.5f * (1.f - base));
  }

  {
    auto channel = lamp();
    channel.setActive(false);
    Core::ChannelColor::step(channel, White, Settings{.highlightBoost = 1.f}, 1.f);

    Hue::Api::ChannelStream stream{.id = 3, .r = 0.3f, .g = 0.3f, .b = 0.8f};
    Core::ChannelColor::shutDown(channel, stream);
    expect("Switched off lamp streams black", stream.id == 3 && stream.r == 0.f && stream.g == 0.f && stream.b == 0.f);
    expect("Switched off lamp becomes inactive", channel.state == Hue::Api::Channel::State::Inactive);
    expect("Switched off lamp forgets its easing", !channel.hasPreviousXyb && !channel.hasPreviousMeasuredBrightness && channel.highlightBoost == 0.f);

    // Switched on again: no easing from the old color
    channel.setActive(true);
    expectNear("Switched on again snaps", Core::ChannelColor::step(channel, Black, Settings{.transitionSmoothing = 0.9f}, 0.f).z, 0.f);
  }

  return failures == 0 ? 0 : 1;
}
