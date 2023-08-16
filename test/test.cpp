#include "PAZ_Audio"
#include <cmath>
#include <thread>

#ifndef M_PI
#define M_PI 3.14159265358979323846264338328
#endif

static constexpr std::size_t SampleRate = 44'100;
static constexpr double Amp = 0.15;
static constexpr std::array<double, 3> Ratios = {1., 1.2, 1.5}; // Minor triad

int main()
{
    std::vector<paz::AudioTrack> tracks;
    for(auto n : Ratios)
    {
        std::vector<float> samples(100/n);
        for(std::size_t i = 0; i < samples.size(); ++i)
        {
            samples[i] = Amp/n*std::sin(i*2.*M_PI/(samples.size() - 1));
        }
        tracks.emplace_back(samples);
    }
    paz::AudioEngine::Play(tracks[0], true);
    paz::AudioEngine::Play(tracks[1], true);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    paz::AudioEngine::Play(tracks[2], true);
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
    paz::AudioEngine::Play(tracks[0], true);
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
    paz::AudioEngine::SetVolume(0., 0.5);
    std::this_thread::sleep_for(std::chrono::milliseconds(750));
}
