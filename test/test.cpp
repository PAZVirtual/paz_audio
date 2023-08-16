#include "PAZ_Audio"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846264338328
#endif

static constexpr std::size_t SampleRate = 44'100;
static constexpr double Freq = 440.;
static constexpr double Amp = 0.5;

int main()
{
    std::vector<float> samples(SampleRate);
    for(std::size_t i = 0; i < SampleRate; ++i)
    {
        samples[i] = Amp*std::sin(Freq*2.*M_PI*static_cast<double>(i)/
            (SampleRate - 1));
    }
    paz::AudioTrack track(samples, SampleRate);
    paz::AudioEngine::Play(track, false);
}
