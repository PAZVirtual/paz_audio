#include "PAZ_Audio"
#include "audio_engine.hpp"
#include "detect_os.hpp"
#include <portaudio.h>
#ifdef PAZ_LINUX
#include <alsa/error.h> // Just for error redirection below
#endif
#include <thread>
#include <mutex>

static PaStream* Stream;
static constexpr double SampleRate = 44'100;
static constexpr unsigned long FramesPerBuf = 256/4;
static std::mutex Mx;
static std::array<std::uint8_t, 2> MasterVol;
// [samplesPtr, sampleIdx, loop]
static std::vector<std::tuple<std::uintptr_t, std::size_t, bool>> ActiveTracks;

static int audio_callback_stereo(const void*, void* outBuf, unsigned long
    framesPerBuffer, const PaStreamCallbackTimeInfo*, PaStreamCallbackFlags,
    void*)
{
    static std::array<std::uint8_t, 2> vol;
    auto* out = static_cast<float*>(outBuf);
    std::fill(out, out + 2*framesPerBuffer, 0);
    std::lock_guard<std::mutex> lk(Mx);
    for(unsigned long i = 0; i < framesPerBuffer; ++i)
    {
        std::size_t j = 0;
        while(j < ActiveTracks.size())
        {
            const auto* samples = reinterpret_cast<const std::vector<float>*>(
                std::get<0>(ActiveTracks[j]));
            for(int k = 0; k < 2; ++k)
            {
                out[2*i + k] += (*samples)[std::get<1>(ActiveTracks[j])];
            }
            ++std::get<1>(ActiveTracks[j]);
            if(std::get<1>(ActiveTracks[j]) == samples->size())
            {
                if(std::get<2>(ActiveTracks[j]))
                {
                    std::get<1>(ActiveTracks[j]) = 0;
                    ++j;
                }
                else
                {
                    std::swap(ActiveTracks[j], ActiveTracks.back());
                    ActiveTracks.pop_back();
                }
            }
            else
            {
                ++j;
            }
        }
        for(int j = 0; j < 2; ++j)
        {
            if(vol[j] < MasterVol[j])
            {
                ++vol[j];
            }
            else if(vol[j] > MasterVol[j])
            {
                --vol[j];
            }
            const float v = vol[j]/255.;
            out[2*i + j] = std::max(-1.f, std::min(1.f, out[2*i + j]*v*v));
        }
    }
    return 0;
}

paz::AudioInitializer& paz::init_audio()
{
    static AudioInitializer initializer;
    return initializer;
}

#ifdef PAZ_LINUX
static void alsa_error_handler(const char*, int, const char*, int, const char*,
    ...) {}
#endif

paz::AudioInitializer::AudioInitializer()
{
#ifdef PAZ_LINUX
    // Redirect ALSA output on PortAudio initialization.
    snd_lib_error_set_handler(alsa_error_handler);
#endif

    PaError error;

    error = Pa_Initialize();
    if(error != paNoError)
    {
        throw std::runtime_error("Failed to initialize PortAudio: " + std::
            string(Pa_GetErrorText(error)));
    }

    error = Pa_OpenDefaultStream(&Stream, 0, 2, paFloat32, SampleRate,
        FramesPerBuf, audio_callback_stereo, nullptr);
    if(error != paNoError)
    {
        throw std::runtime_error("Failed to open audio stream: " + std::string(
            Pa_GetErrorText(error)));
    }

    error = Pa_StartStream(Stream);
    if(error != paNoError)
    {
        throw std::runtime_error("Failed to start audio stream: " + std::string(
            Pa_GetErrorText(error)));
    }
}

paz::AudioInitializer::~AudioInitializer()
{
    Pa_StopStream(Stream);
    Pa_CloseStream(Stream);
    Pa_Terminate();
}

void paz::AudioEngine::Play(const paz::AudioTrack& track, bool loop)
{
    init_audio();

    std::thread([track, loop]()
    {
        std::lock_guard<std::mutex> lk(Mx);
        ActiveTracks.emplace_back(reinterpret_cast<std::uintptr_t>(track.
            _samples.get()), 0, loop);
    }
    ).detach();
}

void paz::AudioEngine::SetVolume(double vol, int ear)
{
    vol = std::max(0., std::min(1., vol));
    if(ear < 0)
    {
        MasterVol[0] = vol*255;
        MasterVol[1] = vol*255;
    }
    else if(ear == 0 || ear == 1)
    {
        MasterVol[ear] = vol*255;
    }
}
