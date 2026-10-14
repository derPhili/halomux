#include <Huenicorn/Hue/Api/Channel.hpp>

#include <glm/common.hpp>

#include <Huenicorn/Serialization/Channel.hpp>


namespace Huenicorn::Hue::Api
{
  Channel::Channel(
    bool active,
    const Devices& devices,
    float gammaFactor,
    const Imaging::UVs& uvs
  ):
  state(active ? State::Active : State::Inactive),
  devices(devices),
  gammaFactor(gammaFactor),
  uvs(uvs)
  {}


  void Channel::setActive(bool active)
  {
    if(active){
      state = State::Active;
    }
    else{
      state = State::PendingShutdown;
    }
  }


  Imaging::UVs& Channel::setUV(
    const Imaging::UV& uv,
    Imaging::UVCorner uvCorner
  )
  {
    float x = glm::clamp(uv.x, 0.f, 1.f);
    float y = glm::clamp(uv.y, 0.f, 1.f);

    switch(uvCorner)
    {
      case Imaging::UVCorner::TopLeft:
        uvs.min = {x, y};
        uvs.max.x = glm::max(x, uvs.max.x);
        uvs.max.y = glm::max(y, uvs.max.y);
        break;

      case Imaging::UVCorner::TopRight:
        uvs.max.x = x;
        uvs.min.y = y;
        uvs.min.x = glm::min(x, uvs.min.x);
        uvs.max.y = glm::max(y, uvs.max.y);
        break;

      case Imaging::UVCorner::BottomLeft:
        uvs.min.x = x;
        uvs.max.y = y;
        uvs.max.x = glm::max(x, uvs.max.x);
        uvs.min.y = glm::min(y, uvs.min.y);
        break;

      case Imaging::UVCorner::BottomRight:
        uvs.max = {x, y};
        uvs.min.x = glm::min(x, uvs.min.x);
        uvs.min.y = glm::min(y, uvs.min.y);
        break;
    }

    return uvs;
  }


  void Channel::acknowledgeShutdown()
  {
    state = State::Inactive;
  }


  float Channel::updateHighlightBoost(
    float measuredBrightness,
    float strength,
    float decay
  )
  {
    const float rise = hasPreviousMeasuredBrightness ? measuredBrightness - previousMeasuredBrightness : 0.f;
    previousMeasuredBrightness = measuredBrightness;
    hasPreviousMeasuredBrightness = true;

    const float trigger = glm::clamp((rise - BoostThreshold) / (BoostFullJump - BoostThreshold), 0.f, 1.f);

    // Instant attack, exponential release
    highlightBoost = glm::max(highlightBoost * decay, glm::clamp(strength, 0.f, 1.f) * trigger);

    return highlightBoost;
  }
}
