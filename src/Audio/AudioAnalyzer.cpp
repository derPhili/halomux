#include <Huenicorn/Audio/AudioAnalyzer.hpp>

#include <algorithm>
#include <cmath>
#include <future>

#include <Huenicorn/Core/Logger.hpp>


namespace Huenicorn::Audio
{
  AudioAnalyzer::AudioAnalyzer()
  {
    std::promise<void> loopReady;
    auto loopReadyFuture = loopReady.get_future();

    m_thread.emplace([this, &loopReady](){
      pw_init(nullptr, nullptr);
      m_loop = pw_main_loop_new(nullptr);
      loopReady.set_value();

      _run();

      pw_main_loop_destroy(m_loop);
      pw_deinit();
    });

    // The loop must exist before the destructor may quit it
    loopReadyFuture.wait();
  }


  AudioAnalyzer::~AudioAnalyzer()
  {
    if(m_loop){
      pw_main_loop_quit(m_loop);
    }

    if(m_thread.has_value()){
      m_thread->join();
    }
  }


  AudioAnalyzer::Triggers AudioAnalyzer::takeTriggers()
  {
    return {m_triggers[0].exchange(0.f), m_triggers[1].exchange(0.f)};
  }


  void AudioAnalyzer::_run()
  {
    pw_stream_events streamEvents = {};
    streamEvents.version = PW_VERSION_STREAM_EVENTS;
    streamEvents.param_changed = _onStreamParamChanged;
    streamEvents.process = _onStreamProcess;

    // Capture what is played on the default output, not a microphone
    auto props = pw_properties_new(
      PW_KEY_MEDIA_TYPE, "Audio",
      PW_KEY_MEDIA_CATEGORY, "Capture",
      PW_KEY_MEDIA_ROLE, "Music",
      PW_KEY_STREAM_CAPTURE_SINK, "true",
      PW_KEY_NODE_NAME, "HalomuxAudio",
      NULL
    );

    m_stream = pw_stream_new_simple(pw_main_loop_get_loop(m_loop), "HalomuxAudio", props, &streamEvents, this);

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
    }
    else{
      Core::Logger::log("[audio] Listening to the default audio output");
      pw_main_loop_run(m_loop);
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
