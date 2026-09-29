#ifndef DEERS_AUDIO_HPP
#define DEERS_AUDIO_HPP

#include <stdint.h>
#include <string>
#include <vector>


class AudioLoop
{
public:

    AudioLoop();

    ~AudioLoop();


    bool load(
        const std::string& path
    );


    bool start();

    void stop();

    void toggle();

    void setEnabled(
        bool enabled
    );


    bool enabled() const;

    bool loaded() const;


private:

    std::vector<int16_t> pcm_;

    unsigned int frames_;

    volatile bool running_;
    volatile bool enabled_;

    int channel_;
    int threadId_;

    unsigned int position_;


    static AudioLoop* instance_;


    static int threadEntry(
        unsigned int args,
        void* argp
    );


    int run();
};

#endif