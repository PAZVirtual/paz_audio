#include "PAZ_Audio"
#include <cmath>
#include <thread>

#ifndef M_PI
#define M_PI 3.14159265358979323846264338328
#endif
#ifndef M_SQRT1_2
#define M_SQRT1_2 0.70710678118654752440084436210
#endif

static constexpr std::size_t SampleRate = 44'100;
static constexpr double Amp = 0.35;
static const std::array<double, 3> Ratios = {1., 1.1892, 1.4983}; // Minor triad

int main()
{
    std::vector<paz::AudioTrack> tracks;
    // Triangle wave. Ensuring each track starts at zero prevents popping.
    for(auto n : Ratios)
    {
        std::vector<float> samples(100/n);
        for(std::size_t i = 0; i < samples.size(); ++i)
        {
            const auto a = samples.size()/4;
            const auto b = samples.size()/2;
            const auto c = 3*samples.size()/4;
            if(i < a)
            {
                samples[i] = static_cast<double>(i)/a;
            }
            else if(i < b)
            {
                samples[i] = 1. - static_cast<double>(i - a)/(b - a);
            }
            else if(i < c)
            {
                samples[i] = -static_cast<double>(i - b)/(c - b);
            }
            else
            {
                samples[i] = -1. + static_cast<double>(i - c)/(samples.size() -
                    c);
            }
            samples[i] *= Amp/n;
        }
        tracks.emplace_back(samples);
    }

    paz::AudioEngine::Play(tracks[0], true);
    paz::AudioEngine::Play(tracks[1], true);
    paz::AudioEngine::SetVolume(M_SQRT1_2);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    paz::AudioEngine::SetVolume(1., 0);
    paz::AudioEngine::SetVolume(0., 1);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    paz::AudioEngine::Play(tracks[2], true);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    paz::AudioEngine::SetVolume(1., 1);
    paz::AudioEngine::SetVolume(0., 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    paz::AudioEngine::Play(tracks[0], true);
    paz::AudioEngine::SetVolume(M_SQRT1_2);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    paz::AudioEngine::SetFreqScale(Ratios[2]/Ratios[0]);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    paz::AudioEngine::SetFreqScale(1.);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    paz::AudioEngine::SetVolume(0.); //TEMP - audio engine should wait for stream to go to zero before terminating
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}
