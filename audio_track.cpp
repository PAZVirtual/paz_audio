#include "PAZ_Audio"

paz::AudioTrack::AudioTrack(const std::vector<float>& samples, std::size_t
    sampleRate) : _samples(samples), _sampleRate(sampleRate) {}
