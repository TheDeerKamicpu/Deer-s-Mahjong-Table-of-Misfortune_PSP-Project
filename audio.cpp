#include "audio.hpp"

#include <pspaudio.h>
#include <pspkernel.h>

#include <stdio.h>
#include <string.h>


AudioLoop* AudioLoop::instance_ =
    0;


AudioLoop::AudioLoop()
    : frames_(0),
      running_(false),
      enabled_(true),
      channel_(-1),
      threadId_(-1),
      position_(0)
{
}


AudioLoop::~AudioLoop()
{
    stop();
}


static uint16_t readLE16(
    const unsigned char* p
)
{
    return
        (uint16_t)p[0] |
        (
            (uint16_t)p[1]
            << 8
        );
}


static uint32_t readLE32(
    const unsigned char* p
)
{
    return
        (uint32_t)p[0] |
        (
            (uint32_t)p[1]
            << 8
        ) |
        (
            (uint32_t)p[2]
            << 16
        ) |
        (
            (uint32_t)p[3]
            << 24
        );
}


bool AudioLoop::load(
    const std::string& path
)
{
    stop();


    pcm_.clear();

    frames_ =
        0;

    position_ =
        0;


    FILE* file =
        fopen(
            path.c_str(),
            "rb"
        );


    if (!file)
        return false;


    unsigned char riff[12];


    if (
        fread(
            riff,
            1,
            12,
            file
        ) != 12
    )
    {
        fclose(file);

        return false;
    }


    if (
        memcmp(
            riff,
            "RIFF",
            4
        ) != 0 ||
        memcmp(
            riff + 8,
            "WAVE",
            4
        ) != 0
    )
    {
        fclose(file);

        return false;
    }


    uint16_t audioFormat =
        0;

    uint16_t channels =
        0;

    uint32_t sampleRate =
        0;

    uint16_t bits =
        0;


    std::vector<unsigned char>
        data;


    while (!feof(file))
    {
        unsigned char chunk[8];


        if (
            fread(
                chunk,
                1,
                8,
                file
            ) != 8
        )
        {
            break;
        }


        uint32_t size =
            readLE32(
                chunk + 4
            );


        if (
            memcmp(
                chunk,
                "fmt ",
                4
            ) == 0
        )
        {
            std::vector<unsigned char>
                fmt(
                    size
                );


            if (
                fread(
                    &fmt[0],
                    1,
                    size,
                    file
                ) != size
            )
            {
                fclose(file);

                return false;
            }


            if (
                size < 16
            )
            {
                fclose(file);

                return false;
            }


            audioFormat =
                readLE16(
                    &fmt[0]
                );


            channels =
                readLE16(
                    &fmt[2]
                );


            sampleRate =
                readLE32(
                    &fmt[4]
                );


            bits =
                readLE16(
                    &fmt[14]
                );
        }

        else if (
            memcmp(
                chunk,
                "data",
                4
            ) == 0
        )
        {
            data.resize(
                size
            );


            if (
                size > 0 &&
                fread(
                    &data[0],
                    1,
                    size,
                    file
                ) != size
            )
            {
                fclose(file);

                return false;
            }
        }

        else
        {
            fseek(
                file,
                size,
                SEEK_CUR
            );
        }


        if (
            size & 1
        )
        {
            fseek(
                file,
                1,
                SEEK_CUR
            );
        }
    }


    fclose(file);


    if (
        audioFormat != 1 ||
        channels != 2 ||
        sampleRate != 44100 ||
        bits != 16 ||
        data.empty()
    )
    {
        return false;
    }


    unsigned int sampleCount =
        data.size() / 2;


    pcm_.resize(
        sampleCount
    );


    for (
        unsigned int i = 0;
        i < sampleCount;
        ++i
    )
    {
        pcm_[i] =
            (int16_t)
            readLE16(
                &data[
                    i * 2
                ]
            );
    }


    frames_ =
        sampleCount / 2;


    return (
        frames_ > 0
    );
}


bool AudioLoop::loaded() const
{
    return (
        !pcm_.empty() &&
        frames_ > 0
    );
}


bool AudioLoop::start()
{
    if (!loaded())
        return false;


    if (running_)
        return true;


    channel_ =
        sceAudioChReserve(
            PSP_AUDIO_NEXT_CHANNEL,
            1024,
            PSP_AUDIO_FORMAT_STEREO
        );


    if (
        channel_ < 0
    )
    {
        return false;
    }


    running_ =
        true;

    position_ =
        0;

    instance_ =
        this;


    threadId_ =
        sceKernelCreateThread(
            "deers_audio",
            threadEntry,
            0x18,
            0x10000,
            PSP_THREAD_ATTR_USER,
            0
        );


    if (
        threadId_ < 0
    )
    {
        running_ =
            false;

        sceAudioChRelease(
            channel_
        );

        channel_ =
            -1;

        return false;
    }


    sceKernelStartThread(
        threadId_,
        0,
        0
    );


    return true;
}


void AudioLoop::stop()
{
    if (!running_)
        return;


    running_ =
        false;


    if (
        threadId_ >= 0
    )
    {
        sceKernelWaitThreadEnd(
            threadId_,
            0
        );


        sceKernelDeleteThread(
            threadId_
        );


        threadId_ =
            -1;
    }


    if (
        channel_ >= 0
    )
    {
        sceAudioChRelease(
            channel_
        );


        channel_ =
            -1;
    }


    if (
        instance_ == this
    )
    {
        instance_ =
            0;
    }
}


void AudioLoop::toggle()
{
    enabled_ =
        !enabled_;
}


void AudioLoop::setEnabled(
    bool enabled
)
{
    enabled_ =
        enabled;
}


bool AudioLoop::enabled() const
{
    return enabled_;
}


int AudioLoop::threadEntry(
    unsigned int args,
    void* argp
)
{
    if (!instance_)
        return 0;


    return instance_->run();
}


int AudioLoop::run()
{
    static const int BLOCK =
        1024;


    int16_t buffer[
        BLOCK * 2
    ];


    while (running_)
    {
        if (!enabled_)
        {
            sceKernelDelayThread(
                20000
            );

            continue;
        }


        for (
            int frame = 0;
            frame < BLOCK;
            ++frame
        )
        {
            if (
                position_ >=
                frames_
            )
            {
                position_ =
                    0;
            }


            unsigned int sample =
                position_ * 2;


            buffer[
                frame * 2
            ] =
                pcm_[
                    sample
                ];


            buffer[
                frame * 2 + 1
            ] =
                pcm_[
                    sample + 1
                ];


            ++position_;
        }


        sceAudioOutputBlocking(
            channel_,
            PSP_AUDIO_VOLUME_MAX,
            buffer
        );
    }


    return 0;
}