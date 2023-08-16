#include "PAZ_Audio"
#include "audio_engine.hpp"
#include "portaudio.h"
#include <iostream>

static int audio_callback_stereo(const void* /* inBuf */, void* outBuf, unsigned
    long framesPerBuffer, const PaStreamCallbackTimeInfo* /* timeInfo */,
    PaStreamCallbackFlags /* statusFlags */, void* data)
{
    auto* track = static_cast<paz::AudioTrack*>(data);
    auto* out = static_cast<float*>(outBuf);
    for(decltype(framesPerBuffer) i = 0; i < framesPerBuffer; ++i)
    {
        out[2*i + 0] = track->_samples[track->_idx]; // Left
        out[2*i + 1] = track->_samples[track->_idx]; // Right
        track->_idx = (track->_idx + 1)%track->_samples.size();
    }
    return 0;
}

paz::AudioInitializer& paz::init_audio()
{
    static AudioInitializer initializer;
    return initializer;
}

paz::AudioInitializer::AudioInitializer()
{
    const auto err = Pa_Initialize();
    if(err != paNoError)
    {
        throw std::runtime_error("Failed to initialize PortAudio: " + std::
            string(Pa_GetErrorText(err)));
    }
}

paz::AudioInitializer::~AudioInitializer()
{
    Pa_Terminate();
}

static std::vector<PaStream*> Streams;

void paz::AudioEngine::Play(paz::AudioTrack& track, bool /* loop */)
{
    init_audio();

    if(track._samples.empty())
    {
        return;
    }

    track._idx = 0;

    Streams.emplace_back();
    {
        const auto err = Pa_OpenDefaultStream(&Streams.back(),
                                0,          /* no input channels */
                                2,          /* stereo output */
                                paFloat32,  /* 32 bit floating point output */
                                track._sampleRate,
                                256,        /* frames per buffer */
                                audio_callback_stereo,
                                &track);
        if( err != paNoError ) throw std::runtime_error("A");
    }

    {
        const auto err = Pa_StartStream(Streams.back());
        if( err != paNoError ) throw std::runtime_error("B");
    }

    Pa_Sleep(1000);

    {
        const auto err = Pa_StopStream(Streams.back());
        if( err != paNoError ) throw std::runtime_error("C");
    }
    {
        const auto err = Pa_CloseStream(Streams.back());
        if( err != paNoError ) throw std::runtime_error("D");
    }
    Streams.pop_back();
}
