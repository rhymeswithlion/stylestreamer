#include "engine/PromptPortfolio.h"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <sstream>

namespace mrt::plugin
{
namespace
{

std::string trim (std::string value)
{
    auto isSpace = [] (unsigned char c) { return std::isspace (c) != 0; };

    value.erase (value.begin(), std::find_if (value.begin(), value.end(),
                                    [&] (unsigned char c) { return ! isSpace (c); }));
    value.erase (std::find_if (value.rbegin(), value.rend(),
                    [&] (unsigned char c) { return ! isSpace (c); })
                    .base(),
        value.end());
    return value;
}

float clampWeight (float weight)
{
    return std::clamp (weight, 0.0f, 2.0f);
}

} // namespace

PromptPortfolio::PromptPortfolio (std::size_t slotCount)
    : slots_ (slotCount)
{
}

void PromptPortfolio::setSlot (std::size_t index, std::string text, float weight)
{
    if (index >= slots_.size())
        return;

    slots_[index] = PromptSlot { trim (std::move (text)), clampWeight (weight) };
}

const std::vector<PromptSlot>& PromptPortfolio::slots() const noexcept
{
    return slots_;
}

std::vector<PromptSlot> PromptPortfolio::activeSlots() const
{
    std::vector<PromptSlot> active;
    active.reserve (slots_.size());

    for (const auto& slot : slots_)
    {
        if (! slot.text.empty() && slot.weight > 0.0f)
            active.push_back (slot);
    }

    return active;
}

std::vector<WeightedPrompt> PromptPortfolio::activeSlotsOrdered() const
{
    std::vector<WeightedPrompt> out;
    out.reserve (slots_.size());
    for (std::size_t i = 0; i < slots_.size(); ++i)
    {
        const auto& slot = slots_[i];
        if (! slot.text.empty() && slot.weight > 0.0f)
            out.push_back (WeightedPrompt { i, slot.text, slot.weight, 0.0f });
    }
    return out;
}

std::vector<WeightedPrompt> PromptPortfolio::normalizedActivePrompts() const
{
    const auto ordered = activeSlotsOrdered();
    if (ordered.empty())
        return {};

    float total = 0.0f;
    for (const auto& p : ordered)
        total += p.weight;
    if (total <= 0.0f)
        return {};

    std::vector<WeightedPrompt> normalized = ordered;
    for (auto& p : normalized)
        p.normalizedWeight = p.weight / total;
    return normalized;
}

std::string PromptPortfolio::signature() const
{
    std::ostringstream oss;
    oss << std::fixed << std::setprecision (4);
    bool first = true;
    for (std::size_t i = 0; i < slots_.size(); ++i)
    {
        const auto& slot = slots_[i];
        if (slot.text.empty() || slot.weight <= 0.0f)
            continue;
        if (! first)
            oss << '|';
        first = false;
        oss << i << ':' << slot.text << ':' << slot.weight;
    }
    return oss.str();
}

float PromptPortfolio::totalActiveWeight() const
{
    float total = 0.0f;
    for (const auto& slot : slots_)
        if (! slot.text.empty() && slot.weight > 0.0f)
            total += slot.weight;
    return total;
}

std::optional<PromptSlot> PromptPortfolio::primaryPrompt() const
{
    std::optional<PromptSlot> best;

    for (const auto& slot : activeSlots())
    {
        if (! best.has_value() || slot.weight > best->weight)
            best = slot;
    }

    return best;
}

} // namespace mrt::plugin
