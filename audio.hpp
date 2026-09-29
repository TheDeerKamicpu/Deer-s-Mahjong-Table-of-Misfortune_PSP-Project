#ifndef DEERS_AUDIO_HPP
#define DEERS_AUDIO_HPP

#include <stdint.h>
#include <stdio.h>
#include <atomic>
#include <string>

class AudioLoop
{
public:
    AudioLoop();
    ~AudioLoop();

    bool load(const std::string& path);
    bool start();
    void stop();
    void toggle();
    void setEnabled(bool enabled);
    bool enabled() const;
    bool loaded() const;

private:
    // Resource ownership cannot be copied. Call public control methods from
    // the game thread; only the worker accesses the stream during playback.
    AudioLoop(const AudioLoop&) = delete;
    AudioLoop& operator=(const AudioLoop&) = delete;

    FILE* file_;
    long dataOffset_;
    uint32_t dataBytes_;
    uint32_t position_;
    std::atomic<bool> running_;
    std::atomic<bool> enabled_;
    int channel_;
    int threadId_;

    static int threadEntry(unsigned int args, void* argp);
    bool readBlock(unsigned char* buffer, uint32_t bytes);
    int run();
};

#endif
