#include "audio.hpp"

#include <pspaudio.h>
#include <pspkernel.h>

#include <limits.h>
#include <string.h>

namespace {
const int BLOCK_FRAMES = 1024;
const uint32_t FRAME_BYTES = 4; // Stereo signed 16-bit PCM.
const uint32_t BLOCK_BYTES = BLOCK_FRAMES * FRAME_BYTES;

uint16_t readLE16(const unsigned char* p)
{
    return uint16_t(p[0]) | (uint16_t(p[1]) << 8);
}

uint32_t readLE32(const unsigned char* p)
{
    return uint32_t(p[0]) | (uint32_t(p[1]) << 8) |
           (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}

// Inspect chunk headers only. Never allocate or read the entire data chunk.
bool inspectWav(FILE* file, long& dataOffset, uint32_t& dataBytes)
{
    if (fseek(file, 0, SEEK_END) != 0)
        return false;
    const long fileBytes = ftell(file);
    if (fileBytes < 12 || fseek(file, 0, SEEK_SET) != 0)
        return false;

    unsigned char riff[12];
    if (fread(riff, 1, sizeof(riff), file) != sizeof(riff) ||
        memcmp(riff, "RIFF", 4) != 0 || memcmp(riff + 8, "WAVE", 4) != 0)
        return false;

    const uint64_t riffEnd = uint64_t(readLE32(riff + 4)) + 8;
    if (riffEnd < 12 || riffEnd > uint64_t(fileBytes) || riffEnd > LONG_MAX)
        return false;

    bool haveFormat = false;
    bool haveData = false;
    uint64_t offset = 12;
    while (offset + 8 <= riffEnd)
    {
        if (fseek(file, static_cast<long>(offset), SEEK_SET) != 0)
            return false;
        unsigned char chunk[8];
        if (fread(chunk, 1, sizeof(chunk), file) != sizeof(chunk))
            return false;
        const uint32_t size = readLE32(chunk + 4);
        const uint64_t payload = offset + 8;
        const uint64_t next = payload + uint64_t(size) + (size & 1u);
        if (next > riffEnd)
            return false;

        if (memcmp(chunk, "fmt ", 4) == 0 && !haveFormat)
        {
            unsigned char fmt[16];
            if (size < sizeof(fmt) ||
                fread(fmt, 1, sizeof(fmt), file) != sizeof(fmt))
                return false;
            // Matches tools/build_assets.py: pcm_s16le, stereo, 44100 Hz.
            if (readLE16(fmt) != 1 || readLE16(fmt + 2) != 2 ||
                readLE32(fmt + 4) != 44100 || readLE32(fmt + 8) != 176400 ||
                readLE16(fmt + 12) != FRAME_BYTES || readLE16(fmt + 14) != 16)
                return false;
            haveFormat = true;
        }
        else if (memcmp(chunk, "data", 4) == 0 && !haveData)
        {
            if (size == 0 || size % FRAME_BYTES != 0)
                return false;
            dataOffset = static_cast<long>(payload);
            dataBytes = size;
            haveData = true;
        }

        if (haveFormat && haveData)
            return fseek(file, dataOffset, SEEK_SET) == 0;
        offset = next; // Includes the RIFF padding byte for odd chunks.
    }
    return false;
}
} // namespace

AudioLoop::AudioLoop()
    : file_(0), dataOffset_(0), dataBytes_(0), position_(0),
      running_(false), enabled_(true), channel_(-1), threadId_(-1)
{
}

AudioLoop::~AudioLoop()
{
    stop();
    if (file_)
        fclose(file_);
}

bool AudioLoop::load(const std::string& path)
{
    stop();
    if (file_)
        fclose(file_);
    file_ = 0;
    dataOffset_ = 0;
    dataBytes_ = 0;
    position_ = 0;

    FILE* candidate = fopen(path.c_str(), "rb");
    if (!candidate)
        return false;
    // Blocks are already buffered by readBlock; avoid another stdio buffer.
    if (setvbuf(candidate, 0, _IONBF, 0) != 0)
    {
        fclose(candidate);
        return false;
    }
    long offset = 0;
    uint32_t bytes = 0;
    if (!inspectWav(candidate, offset, bytes))
    {
        fclose(candidate);
        return false;
    }
    file_ = candidate;
    dataOffset_ = offset;
    dataBytes_ = bytes;
    return true;
}

bool AudioLoop::loaded() const
{
    return file_ != 0 && dataBytes_ != 0;
}

bool AudioLoop::start()
{
    if (!loaded())
        return false;
    if (running_.load())
        return true;
    // Also reap a worker that exited because of an I/O or audio error.
    stop();
    clearerr(file_);
    if (fseek(file_, dataOffset_, SEEK_SET) != 0)
        return false;
    position_ = 0;

    channel_ = sceAudioChReserve(PSP_AUDIO_NEXT_CHANNEL, BLOCK_FRAMES,
                                 PSP_AUDIO_FORMAT_STEREO);
    if (channel_ < 0)
        return false;
    threadId_ = sceKernelCreateThread("deers_audio", threadEntry, 0x18,
                                      0x8000, PSP_THREAD_ATTR_USER, 0);
    if (threadId_ < 0)
    {
        sceAudioChRelease(channel_);
        channel_ = -1;
        return false;
    }
    running_.store(true);
    // The kernel copies these argument bytes to the new thread's stack.
    AudioLoop* self = this;
    if (sceKernelStartThread(threadId_, sizeof(self), &self) < 0)
    {
        running_.store(false);
        sceKernelDeleteThread(threadId_);
        threadId_ = -1;
        sceAudioChRelease(channel_);
        channel_ = -1;
        return false;
    }
    return true;
}

void AudioLoop::stop()
{
    running_.store(false);
    if (threadId_ >= 0)
    {
        // Join before releasing the channel or closing/seeking the stream.
        sceKernelWaitThreadEnd(threadId_, 0);
        sceKernelDeleteThread(threadId_);
        threadId_ = -1;
    }
    if (channel_ >= 0)
    {
        sceAudioChRelease(channel_);
        channel_ = -1;
    }
    // Retain the loaded stream so stop(); start(); still works.
}

void AudioLoop::toggle()
{
    setEnabled(!enabled());
}

void AudioLoop::setEnabled(bool enabled)
{
    enabled_.store(enabled);
}

bool AudioLoop::enabled() const
{
    return enabled_.load();
}

int AudioLoop::threadEntry(unsigned int args, void* argp)
{
    if (args != sizeof(AudioLoop*) || !argp)
        return -1;
    AudioLoop* self = 0;
    memcpy(&self, argp, sizeof(self));
    return self ? self->run() : -1;
}

bool AudioLoop::readBlock(unsigned char* buffer, uint32_t bytes)
{
    uint32_t filled = 0;
    while (filled < bytes)
    {
        if (!running_.load())
            return false;
        if (position_ == dataBytes_)
        {
            if (fseek(file_, dataOffset_, SEEK_SET) != 0)
                return false;
            position_ = 0;
        }
        uint32_t count = dataBytes_ - position_;
        if (count > bytes - filled)
            count = bytes - filled;
        // A short read means truncation/I/O failure, not the loop boundary.
        if (fread(buffer + filled, 1, count, file_) != count)
            return false;
        filled += count;
        position_ += count;
    }
    return true;
}

int AudioLoop::run()
{
    // PSP is little endian, matching pcm_s16le. Two aligned 4 KiB blocks
    // keep the submitted buffer untouched while the next one is filled.
    int16_t buffers[2][BLOCK_FRAMES * 2] __attribute__((aligned(64)));
    unsigned int current = 0;
    while (running_.load())
    {
        if (!enabled_.load())
        {
            sceKernelDelayThread(20000);
            continue; // Pause the stream position, as in the original player.
        }
        if (!readBlock(reinterpret_cast<unsigned char*>(buffers[current]), BLOCK_BYTES))
            break;
        if (!running_.load())
            break;
        if (sceAudioOutputBlocking(channel_, PSP_AUDIO_VOLUME_MAX,
                                   buffers[current]) < 0)
            break;
        current ^= 1;
    }
    // The last submitted buffer may still be playing. Keep its stack alive
    // until the channel is drained, including on stop and read failures.
    while (sceAudioGetChannelRestLen(channel_) > 0)
        sceKernelDelayThread(1000);
    running_.store(false);
    return 0;
}
