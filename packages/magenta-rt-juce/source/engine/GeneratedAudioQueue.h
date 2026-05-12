#pragma once

#include <atomic>
#include <cstddef>
#include <vector>

namespace mrt::plugin
{

class GeneratedAudioQueue
{
public:
    GeneratedAudioQueue (int numChannels, std::size_t capacityFrames);

    [[nodiscard]] int numChannels() const noexcept;
    [[nodiscard]] std::size_t capacityFrames() const noexcept;
    [[nodiscard]] std::size_t queuedFrames() const noexcept;

    bool pushInterleaved (const float* samples, std::size_t frames, int inputChannels);
    std::size_t popToDeinterleaved (float* const* outputs, int outputChannels, std::size_t frames);
    void clear() noexcept;

private:
    [[nodiscard]] std::size_t nextFrame (std::size_t frame) const noexcept;
    [[nodiscard]] std::size_t writableFrames (std::size_t readFrame, std::size_t writeFrame) const noexcept;

    int numChannels_ = 0;
    std::size_t capacityFrames_ = 0;
    std::vector<float> buffer_;
    std::atomic<std::size_t> readFrame_ { 0 };
    std::atomic<std::size_t> writeFrame_ { 0 };
};

} // namespace mrt::plugin
