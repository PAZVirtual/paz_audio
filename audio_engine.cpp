#include "PAZ_Audio"
#include "audio_engine.hpp"
#include "detect_os.hpp"
#ifdef PAZ_MACOS
#include <CoreAudio/CoreAudio.h>
#include <AudioUnit/AudioUnit.h>
#else
#include <portaudio.h>
#ifdef PAZ_LINUX
#include <alsa/error.h> // Just for error redirection below
#endif
#endif
#include <thread>
#include <mutex>
#include <condition_variable>
#include <cmath>
#include <stdexcept>

#ifdef PAZ_MACOS
static AudioComponent _outputComp;
static AudioComponentInstance _outputInstance;
#else
static PaStream* _stream;
#endif

static constexpr double SampleRate = 44'100.;
static constexpr unsigned long FramesPerBuf = 1024;

static std::mutex _mx;
static std::condition_variable _cv;
static bool _bufComplete;
static std::array<std::uint8_t, 2> _masterVol;
static std::array<double, 2> _masterFreqScale = {1, 1};
// [samplesPtr, bufStartIndices, loop]
static std::vector<std::tuple<std::uintptr_t, std::array<std::size_t, 2>, bool>>
    _activeTracks;

#ifdef PAZ_MACOS
static OSStatus audio_callback_stereo(void*, AudioUnitRenderActionFlags*, const
    AudioTimeStamp*, UInt32, UInt32 framesPerBuffer, AudioBufferList* buffers)
{
    auto* out = static_cast<float*>(buffers->mBuffers[0].mData);
#else
static int audio_callback_stereo(const void*, void* outBuf, unsigned long
    framesPerBuffer, const PaStreamCallbackTimeInfo*, PaStreamCallbackFlags,
    void*)
{
    auto* out = static_cast<float*>(outBuf);
#endif
    static std::array<std::uint8_t, 2> vol;
    std::fill(out, out + 2*framesPerBuffer, 0);
    std::lock_guard<std::mutex> lk(_mx);
    for(unsigned long i = 0; i < framesPerBuffer; ++i)
    {
        for(const auto& n : _activeTracks)
        {
            const auto* samples = reinterpret_cast<const std::vector<float>*>(
                std::get<0>(n));
            for(int k = 0; k < 2; ++k)
            {
                const std::size_t idx = std::round(std::get<1>(n)[k] + i*
                    _masterFreqScale[k]);
                out[2*i + k] += (*samples)[idx%samples->size()];
            }
        }
        for(int j = 0; j < 2; ++j)
        {
            if(vol[j] < _masterVol[j])
            {
                ++vol[j];
            }
            else if(vol[j] > _masterVol[j])
            {
                --vol[j];
            }
            const float v = vol[j]/255.;
            out[2*i + j] = std::max(-1.f, std::min(1.f, out[2*i + j]*v*v));
        }
    }
    for(auto& n : _activeTracks)
    {
        for(int k = 0; k < 2; ++k)
        {
            std::get<1>(n)[k] = static_cast<std::size_t>(std::round(std::get<1>(
                n)[k] + framesPerBuffer*_masterFreqScale[k]))%reinterpret_cast<
                const std::vector<float>*>(std::get<0>(n))->size();
        }
    }
    if(!_bufComplete)
    {
        _bufComplete = true;
        _cv.notify_all();
    }
#ifdef PAZ_MACOS
    return noErr;
#else
    return 0;
#endif
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
#ifdef PAZ_MACOS
    AudioComponentDescription desc;
    desc.componentType = kAudioUnitType_Output;
    desc.componentSubType = kAudioUnitSubType_DefaultOutput;
    desc.componentFlags = 0;
    desc.componentFlagsMask = 0;
    desc.componentManufacturer = kAudioUnitManufacturer_Apple;

    _outputComp = AudioComponentFindNext(nullptr, &desc);
    if(!_outputComp || AudioComponentInstanceNew(_outputComp, &_outputInstance))
    {
        throw std::runtime_error("Failed to open default audio device.");
    }

    if(AudioUnitInitialize(_outputInstance))
    {
        throw std::runtime_error("Failed to initialize audio unit instance.");
    }

    AudioStreamBasicDescription streamFormat;
    streamFormat.mSampleRate = SampleRate;
    streamFormat.mFormatID = kAudioFormatLinearPCM;
    streamFormat.mFormatFlags = kAudioFormatFlagIsFloat;
    streamFormat.mFramesPerPacket = 1;
    streamFormat.mChannelsPerFrame = 2;
    streamFormat.mBitsPerChannel = 32;
    streamFormat.mBytesPerPacket = 2*sizeof(float);
    streamFormat.mBytesPerFrame = 2*sizeof(float);
    if(AudioUnitSetProperty(_outputInstance, kAudioUnitProperty_StreamFormat,
        kAudioUnitScope_Input, 0, &streamFormat, sizeof(streamFormat)))
    {
        throw std::runtime_error("Failed to set audio unit input property.");
    }

    AURenderCallbackStruct callback;
    callback.inputProc = audio_callback_stereo;
    if(AudioUnitSetProperty(_outputInstance,
        kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, 0,
        &callback, sizeof(AURenderCallbackStruct)))
    {
        throw std::runtime_error("Failed to attach an IOProc to the selected au"
            "dio unit.");
    }

    if(AudioOutputUnitStart(_outputInstance))
    {
        throw std::runtime_error("Failed to start audio unit.");
    }
#else
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

    error = Pa_OpenDefaultStream(&_stream, 0, 2, paFloat32, SampleRate,
        FramesPerBuf, audio_callback_stereo, nullptr);
    if(error != paNoError)
    {
        throw std::runtime_error("Failed to open audio stream: " + std::string(
            Pa_GetErrorText(error)));
    }

    error = Pa_StartStream(_stream);
    if(error != paNoError)
    {
        throw std::runtime_error("Failed to start audio stream: " + std::string(
            Pa_GetErrorText(error)));
    }
#endif

    // Wait for first buffer to be completed.
    {
        std::unique_lock<std::mutex> lk(_mx);
        _cv.wait(lk, [](){ return _bufComplete; });
    }
}

paz::AudioInitializer::~AudioInitializer()
{
#ifdef PAZ_MACOS
    AudioOutputUnitStop(_outputInstance);
    AudioComponentInstanceDispose(_outputInstance);
#else
    Pa_StopStream(_stream);
    Pa_CloseStream(_stream);
    Pa_Terminate();
#endif
}

void paz::AudioEngine::Play(const paz::AudioTrack& track, bool loop)
{
    init_audio();

    if(!loop) throw std::logic_error("NOT IMPLEMENTED");

    std::thread([track, loop]()
    {
        std::lock_guard<std::mutex> lk(_mx);
        _activeTracks.emplace_back(reinterpret_cast<std::uintptr_t>(track.
            _samples.get()), std::array<std::size_t, 2>{}, loop);
    }
    ).detach();
}

void paz::AudioEngine::SetVolume(double vol, int ear)
{
    vol = std::max(0., std::min(1., vol));
    if(ear < 0)
    {
        std::lock_guard<std::mutex> lk(_mx);
        _masterVol[0] = std::round(vol*255.);
        _masterVol[1] = std::round(vol*255.);
    }
    else if(ear == 0 || ear == 1)
    {
        std::lock_guard<std::mutex> lk(_mx);
        _masterVol[ear] = std::round(vol*255.);
    }
}

void paz::AudioEngine::SetFreqScale(double scale, int ear)
{
    if(ear < 0)
    {
        std::lock_guard<std::mutex> lk(_mx);
        _masterFreqScale[0] = scale;
        _masterFreqScale[1] = scale;
    }
    else if(ear == 0 || ear == 1)
    {
        std::lock_guard<std::mutex> lk(_mx);
        _masterFreqScale[ear] = scale;
    }
}
