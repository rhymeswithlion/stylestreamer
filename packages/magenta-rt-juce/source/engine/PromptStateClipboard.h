#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

#include <juce_core/juce_core.h>

namespace mrt::plugin
{

struct PromptStateClipboardSlot
{
    juce::String text;
    double weightPercent = 0.0;
};

struct PromptStateClipboardSettings
{
    juce::String seed;
    double temperature = 1.1;
    int topK = 40;
    double guidanceWeight = 5.0;
    int prebufferChunks = 2;
    int maxQueueChunks = 3;
    double transitionDelaySeconds = 4.0;
};

struct PromptStateClipboardStyleCard
{
    juce::String id;
    juce::String text;
    double weightPercent = 0.0;
    bool active = true;
    std::uint32_t colourArgb = 0xff0891b2;
    juce::String imageKey;
    bool userCreated = false;
    std::optional<int> bankColumn;
    std::optional<int> bankRow;
};

struct PromptStateClipboardPayload
{
    int version = 3;
    std::array<PromptStateClipboardSlot, 4> prompts {};
    std::vector<PromptStateClipboardStyleCard> styleCards;
    PromptStateClipboardSettings settings;
};

struct PromptStateClipboardDecodeResult
{
    bool ok = false;
    PromptStateClipboardPayload payload;
    juce::String error;
};

[[nodiscard]] juce::String encodePromptStateClipboardPayload (
    const PromptStateClipboardPayload& payload);

[[nodiscard]] PromptStateClipboardDecodeResult decodePromptStateClipboardPayload (
    const juce::String& encoded);

} // namespace mrt::plugin
