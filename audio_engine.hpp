#include "PAZ_Audio"

namespace paz
{
    struct AudioData
    {
        std::shared_ptr<std::vector<float>> samples;
        std::size_t idx;
        bool loop;
    };
    struct AudioInitializer
    {
        AudioInitializer();
        ~AudioInitializer();
    };
    AudioInitializer& init_audio();
};
