#include "engine/GeneratedAudioQueue.h"

#include <algorithm>

namespace mrt::plugin
{

GeneratedAudioQueue::GeneratedAudioQueue (int numChannels, std::size_t capacityFrames)
    : numChannels_ (std::max (1, numChannels)),
      capacityFrames_ (capacityFrames + 1),
      buffer_ (capacityFrames_ * static_cast<std::size_t> (numChannels_), 0.0f)
{
}

int GeneratedAudioQueue::numChannels() const noexcept
{
    return numChannels_;
}

std::size_t GeneratedAudioQueue::capacityFrames() const noexcept
{
    return capacityFrames_ - 1;
}

std::size_t GeneratedAudioQueue::queuedFrames() const noexcept
{
    const auto read = readFrame_.load (std::memory_order_acquire);
    const auto write = writeFrame_.load (std::memory_order_acquire);

    if (write >= read)
        return write - read;

    return capacityFrames_ - (read - write);
}

bool GeneratedAudioQueue::pushInterleaved (const float* samples, std::size_t frames, int inputChannels)
{
    if (samples == nullptr || inputChannels != numChannels_)
        return false;

    const auto read = readFrame_.load (std::memory_order_acquire);
    auto write = writeFrame_.load (std::memory_order_relaxed);
    if (frames > writableFrames (read, write))
        return false;

    for (std::size_t frame = 0; frame < frames; ++frame)
    {
        const auto bufferOffset = write * static_cast<std::size_t> (numChannels_);
        const auto inputOffset = frame * static_cast<std::size_t> (inputChannels);
        for (int channel = 0; channel < numChannels_; ++channel)
            buffer_[bufferOffset + static_cast<std::size_t> (channel)] =
                samples[inputOffset + static_cast<std::size_t> (channel)];

        write = nextFrame (write);
    }

    writeFrame_.store (write, std::memory_order_release);
    return true;
}

std::size_t GeneratedAudioQueue::popToDeinterleaved (
    float* const* outputs, int outputChannels, std::size_t frames)
{
    if (outputs == nullptr || outputChannels <= 0)
        return 0;

    auto read = readFrame_.load (std::memory_order_relaxed);
    const auto write = writeFrame_.load (std::memory_order_acquire);
    std::size_t copied = 0;

    for (; copied < frames && read != write; ++copied)
    {
        const auto bufferOffset = read * static_cast<std::size_t> (numChannels_);
        for (int channel = 0; channel < outputChannels; ++channel)
        {
            if (outputs[channel] == nullptr)
                continue;

            outputs[channel][copied] =
                channel < numChannels_ ? buffer_[bufferOffset + static_cast<std::size_t> (channel)]
                                       : 0.0f;
        }

        read = nextFrame (read);
    }

    for (std::size_t frame = copied; frame < frames; ++frame)
    {
        for (int channel = 0; channel < outputChannels; ++channel)
        {
            if (outputs[channel] != nullptr)
                outputs[channel][frame] = 0.0f;
        }
    }

    readFrame_.store (read, std::memory_order_release);
    return copied;
}

void GeneratedAudioQueue::clear() noexcept
{
    readFrame_.store (0, std::memory_order_release);
    writeFrame_.store (0, std::memory_order_release);
}

std::size_t GeneratedAudioQueue::nextFrame (std::size_t frame) const noexcept
{
    return (frame + 1) % capacityFrames_;
}

std::size_t GeneratedAudioQueue::writableFrames (
    std::size_t readFrame, std::size_t writeFrame) const noexcept
{
    if (writeFrame >= readFrame)
        return capacityFrames_ - (writeFrame - readFrame) - 1;

    return readFrame - writeFrame - 1;
}

} // namespace mrt::plugin
