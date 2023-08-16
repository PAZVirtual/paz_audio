#include "PAZ_Audio"
#include "audio_engine.hpp"

paz::AudioTrack::AudioTrack()
{
    init_audio();

    _samples = std::make_shared<std::vector<float>>();
}

paz::AudioTrack::AudioTrack(const std::vector<float>& samples)
{
    init_audio();

    _samples = std::make_shared<std::vector<float>>(samples);
}
