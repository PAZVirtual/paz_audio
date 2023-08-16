#include "PAZ_Audio"
#include <cmath>
#include <thread>

#ifndef M_PI
#define M_PI 3.14159265358979323846264338328
#endif

static constexpr std::size_t SampleRate = 44'100;
static constexpr double Amp = 0.2;

int main()
{
    std::vector<float> samples(100); // = 441 Hz
    for(std::size_t i = 0; i < samples.size(); ++i)
    {
        samples[i] = Amp*std::sin(i*2.*M_PI/(samples.size() - 1));
    }
    paz::AudioTrack track(samples);
    paz::AudioEngine::Play(track, true);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
}
