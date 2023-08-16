#include "PAZ_Audio"
#include "audio_engine.hpp"
#include "detect_os.hpp"
#include <portaudio.h>
#ifdef PAZ_LINUX
#include <alsa/error.h> // Just for error redirection below
#endif
#include <thread>
#include <mutex>
#include <condition_variable>
#include <cmath>

static PaStream* Stream;
static constexpr double SampleRate = 44'100;
static constexpr unsigned long FramesPerBuf = 1024;
static std::mutex Mx;
static std::condition_variable Cv;
static bool BufComplete;
static std::array<std::uint8_t, 2> MasterVol;
static std::array<double, 2> MasterFreqScale = {1, 1};
// [samplesPtr, bufStartIndices, loop]
static std::vector<std::tuple<std::uintptr_t, std::array<std::size_t, 2>, bool>>
    ActiveTracks;

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
        for(const auto& n : ActiveTracks)
        {
            const auto* samples = reinterpret_cast<const std::vector<float>*>(
                std::get<0>(n));
            for(int k = 0; k < 2; ++k)
            {
                const std::size_t idx = std::round(std::get<1>(n)[k] + i*
                    MasterFreqScale[k]);
                out[2*i + k] += (*samples)[idx%samples->size()];
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
    for(auto& n : ActiveTracks)
    {
        for(int k = 0; k < 2; ++k)
        {
            std::get<1>(n)[k] = static_cast<std::size_t>(std::round(std::get<1>(
                n)[k] + framesPerBuffer*MasterFreqScale[k]))%reinterpret_cast<
                const std::vector<float>*>(std::get<0>(n))->size();
        }
    }
    if(!BufComplete)
    {
        BufComplete = true;
        Cv.notify_all();
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


    const auto numDevices = Pa_GetDeviceCount();
    if(numDevices < 0)
    {
        throw std::runtime_error("Failed to get number of audio devices: " +
            std::string(Pa_GetErrorText(numDevices)));
    }
    if(!numDevices)
    {
        throw std::runtime_error("No audio devices found.");
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

    // Wait for first buffer to be completed.
    {
        std::unique_lock<std::mutex> lk(Mx);
        Cv.wait(lk, [](){ return BufComplete; });
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

    if(!loop) throw std::logic_error("NOT IMPLEMENTED");

    std::thread([track, loop]()
    {
        std::lock_guard<std::mutex> lk(Mx);
        ActiveTracks.emplace_back(reinterpret_cast<std::uintptr_t>(track.
            _samples.get()), std::array<std::size_t, 2>{}, loop);
    }
    ).detach();
}

void paz::AudioEngine::SetVolume(double vol, int ear)
{
    vol = std::max(0., std::min(1., vol));
    if(ear < 0)
    {
        std::lock_guard<std::mutex> lk(Mx);
        MasterVol[0] = std::round(vol*255.);
        MasterVol[1] = std::round(vol*255.);
    }
    else if(ear == 0 || ear == 1)
    {
        std::lock_guard<std::mutex> lk(Mx);
        MasterVol[ear] = std::round(vol*255.);
    }
}

void paz::AudioEngine::SetFreqScale(double scale, int ear)
{
    if(ear < 0)
    {
        std::lock_guard<std::mutex> lk(Mx);
        MasterFreqScale[0] = scale;
        MasterFreqScale[1] = scale;
    }
    else if(ear == 0 || ear == 1)
    {
        std::lock_guard<std::mutex> lk(Mx);
        MasterFreqScale[ear] = scale;
    }
}
