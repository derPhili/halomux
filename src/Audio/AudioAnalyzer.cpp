#include <Halomux/Audio/AudioAnalyzer.hpp>

#include <algorithm>
#include <cmath>

#include <Halomux/Core/Logger.hpp>


namespace Halomux::Audio
{
  AudioAnalyzer::AudioAnalyzer()
  {
    m_loopThread.emplace(
      [this](pw_main_loop* loop){return _connect(loop);},
      [this](pw_main_loop*){_disconnect();}
    );
  }


  AudioAnalyzer::~AudioAnalyzer()
  {
    // Quits the loop and waits for its thread
    m_loopThread.reset();
  }


  AudioAnalyzer::Triggers AudioAnalyzer::takeTriggers()
  {
    return {m_triggers[0].exchange(0.f), m_triggers[1].exchange(0.f)};
  }


  bool AudioAnalyzer::_connect(
    pw_main_loop* loop
  )
  {
    // Outlives the stream
    static const pw_stream_events streamEvents = [](){
      pw_stream_events events = {};
      events.version = PW_VERSION_STREAM_EVENTS;
      events.param_changed = _onStreamParamChanged;
      events.process = _onStreamProcess;
      return events;
    }();

    // Capture what is played on the default output, not a microphone
    auto props = pw_properties_new(
      PW_KEY_MEDIA_TYPE, "Audio",
      PW_KEY_MEDIA_CATEGORY, "Capture",
      PW_KEY_MEDIA_ROLE, "Music",
      PW_KEY_STREAM_CAPTURE_SINK, "true",
      PW_KEY_NODE_NAME, "HalomuxAudio",
      NULL
    );

    m_stream = pw_stream_new_simple(pw_main_loop_get_loop(loop), "HalomuxAudio", props, &streamEvents, this);

    uint8_t buffer[1024];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
    spa_pod_builder b = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
    spa_audio_info_raw info = {};
    info.format = SPA_AUDIO_FORMAT_F32;
    info.channels = 2;
    info.position[0] = SPA_AUDIO_CHANNEL_FL;
    info.position[1] = SPA_AUDIO_CHANNEL_FR;
    const spa_pod* params[1] = {spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat, &info)};
#pragma GCC diagnostic pop

    const int result = pw_stream_connect(
      m_stream,
      PW_DIRECTION_INPUT,
      PW_ID_ANY,
      static_cast<pw_stream_flags>(PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS),
      params,
      1
    );

    if(result < 0){
      Core::Logger::error("[audio] Could not capture the audio output");
      return false;
    }

    Core::Logger::log("[audio] Listening to the default audio output");
    return true;
  }


  void AudioAnalyzer::_disconnect()
  {
    if(!m_stream){
      return;
    }

    pw_stream_destroy(m_stream);
    m_stream = nullptr;
  }


  void AudioAnalyzer::_onStreamParamChanged(
    void* userData,
    uint32_t id,
    const spa_pod* param
  )
  {
    auto* self = static_cast<AudioAnalyzer*>(userData);

    if(param == nullptr || id != SPA_PARAM_Format){
      return;
    }

    spa_audio_info_raw info = {};
    if(spa_format_audio_raw_parse(param, &info) < 0){
      return;
    }

    self->m_rate = info.rate > 0 ? info.rate : 48000;
    self->m_channels = std::clamp<uint32_t>(info.channels, 1, 2);
    for(auto& detector : self->m_detectors){
      detector.reset();
    }
  }


  void AudioAnalyzer::_onStreamProcess(
    void* userData
  )
  {
    auto* self = static_cast<AudioAnalyzer*>(userData);

    pw_buffer* pwBuffer = pw_stream_dequeue_buffer(self->m_stream);
    if(!pwBuffer){
      return;
    }

    const spa_data& data = pwBuffer->buffer->datas[0];
    const auto* samples = static_cast<const float*>(data.data);
    const uint32_t channels = self->m_channels;
    const uint32_t frames = (samples && data.chunk) ? data.chunk->size / (sizeof(float) * channels) : 0;

    if(frames > 0){
      samples = reinterpret_cast<const float*>(reinterpret_cast<const uint8_t*>(samples) + data.chunk->offset);

      // Mean power of this buffer per channel (mono feeds both sides)
      std::array<float, 2> energy{0.f, 0.f};
      for(uint32_t frame = 0; frame < frames; frame++){
        for(uint32_t channel = 0; channel < channels; channel++){
          const float sample = samples[frame * channels + channel];
          energy[channel] += sample * sample;
        }
      }

      for(float& channelEnergy : energy){
        channelEnergy /= static_cast<float>(frames);
      }

      if(channels == 1){
        energy[1] = energy[0];
      }

      const float bufferDuration = static_cast<float>(frames) / static_cast<float>(self->m_rate);

      for(size_t channel = 0; channel < 2; channel++){
        const float trigger = self->m_detectors[channel].process(energy[channel], bufferDuration);

        // Keep the strongest trigger until the streaming loop takes it
        float current = self->m_triggers[channel].load();
        while(trigger > current && !self->m_triggers[channel].compare_exchange_weak(current, trigger)){}
      }
    }

    pw_stream_queue_buffer(self->m_stream, pwBuffer);
  }
}
