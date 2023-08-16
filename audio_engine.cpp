#include "PAZ_Audio"
#include "audio_engine.hpp"
#include "detect_os.hpp"
#include <portaudio.h>
#ifdef PAZ_LINUX
#include <alsa/error.h> // Just for error redirection below
#endif
#include <thread>
#include <mutex>

static constexpr double SampleRate = 44'100.;
static constexpr unsigned long FramesPerBuf = 256;
static std::mutex Mx;

static int audio_callback_stereo(const void* /* inBuf */, void* outBuf, unsigned
    long framesPerBuffer, const PaStreamCallbackTimeInfo* /* timeInfo */,
    PaStreamCallbackFlags /* statusFlags */, void* inData)
{
    std::lock_guard<std::mutex> lk(Mx);
    auto* data = static_cast<paz::AudioData*>(inData);
    auto* out = static_cast<float*>(outBuf);
    for(unsigned long i = 0; i < framesPerBuffer; ++i)
    {
        if(data->samples && data->idx < data->samples->size())
        {
            out[2*i + 0] = (*data->samples)[data->idx]; // Left
            out[2*i + 1] = (*data->samples)[data->idx]; // Right
            ++data->idx;
            if(data->loop)
            {
                data->idx %= data->samples->size();
            }
            else
            {
                if(data->idx == data->samples->size())
                {
                    data->samples.reset();
                }
            }
        }
        else
        {
            out[2*i + 0] = 0;
            out[2*i + 1] = 0;
        }
    }
    return 0;
}

static PaStream* Stream;
static paz::AudioData Data;

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
        FramesPerBuf, audio_callback_stereo, &Data);
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

    std::thread thread([track, loop]()
    {
        std::lock_guard<std::mutex> lk(Mx);

        Data.idx = 0;
        Data.samples = track._samples;
        Data.loop = loop;
    }
    );

    thread.detach();
}
