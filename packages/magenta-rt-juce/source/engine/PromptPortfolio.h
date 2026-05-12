#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace mrt::plugin
{

struct PromptSlot
{
    std::string text;
    float weight = 0.0f;
};

struct WeightedPrompt
{
    std::size_t slotIndex = 0;
    std::string text;
    float weight = 0.0f;
    float normalizedWeight = 0.0f;
};

class PromptPortfolio
{
public:
    explicit PromptPortfolio (std::size_t slotCount = 4);

    void setSlot (std::size_t index, std::string text, float weight);
    [[nodiscard]] const std::vector<PromptSlot>& slots() const noexcept;
    [[nodiscard]] std::vector<PromptSlot> activeSlots() const;
    [[nodiscard]] std::vector<WeightedPrompt> activeSlotsOrdered() const;
    [[nodiscard]] std::vector<WeightedPrompt> normalizedActivePrompts() const;
    [[nodiscard]] std::string signature() const;
    [[nodiscard]] float totalActiveWeight() const;
    [[nodiscard]] std::optional<PromptSlot> primaryPrompt() const;

private:
    std::vector<PromptSlot> slots_;
};

} // namespace mrt::plugin
