#include <Huenicorn/Timing/LoopRegulator.hpp>

#include <thread>


namespace Huenicorn::Timing
{
  LoopRegulator::LoopRegulator(
    Duration tickInterval
  ):
  m_tickInterval(tickInterval)
  {
    std::fill(m_loadRateHistory, m_loadRateHistory + LoadRateHistorySize, Duration{0.0});
  }


  Duration LoopRegulator::tickInterval() const
  {
    return m_tickInterval;
  }


  const LoopRegulator::Excess& LoopRegulator::lastExcess() const
  {
    return m_lastExcess;
  }


  float LoopRegulator::loadRate(
    bool asPercents
  ) const
  {
    return asPercents ? m_loadRate * 100 : m_loadRate;
  }


  void LoopRegulator::setTickInterval(
    Duration tickInterval
  )
  {
    m_tickInterval = tickInterval;
  }


  void LoopRegulator::start()
  {
    m_nextTickTime = ClockType::now();
  }


  bool LoopRegulator::sync()
  {
    Duration excess = _syncWithTick(m_nextTickTime);
    Duration tickInterval = m_tickInterval;
    int factor = 1;

    if(excess.count() > 0.0 && tickInterval.count() > 0.0){
      factor += static_cast<int>(excess / tickInterval);
    }

    m_nextTickTime += std::chrono::duration_cast<ClockType::duration>(tickInterval * factor);

    return factor <= 2;
  }


  Duration LoopRegulator::_syncWithTick(
    const TimePoint& startTime
  )
  {
    // Measure
    TimePoint now = ClockType::now();
    m_loadRate = _computeLoad(startTime, now);

    // Sync
    Duration tickInterval = m_tickInterval;
    if(tickInterval.count() == 0){
      return Duration{0};
    }

    TimePoint next = startTime + std::chrono::duration_cast<ClockType::duration>(tickInterval);

    if(next > now){
      std::this_thread::sleep_until(next);
      return Duration{0};
    }

    Duration excess = std::chrono::duration_cast<Duration>(now - next);

    float ratio = static_cast<float>(excess.count() / tickInterval.count());
    m_lastExcess = {excess, ratio};

    return excess;
  }
}
