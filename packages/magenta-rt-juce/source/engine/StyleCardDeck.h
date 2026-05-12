#pragma once

#include "engine/PromptPortfolio.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace mrt::plugin
{

struct StyleCardTransition
{
    bool inProgress = false;
    bool fromActive = true;
    bool toActive = true;
    double startSeconds = 0.0;
    double durationSeconds = 4.0;
};

struct StyleCardBankPosition
{
    int column = 0;
    int row = 0;
};

struct StyleCard
{
    std::string id;
    std::string text;
    float weight = 0.0f;
    bool active = true;
    std::uint32_t colourArgb = 0xff0891b2;
    std::string imageKey;
    bool userCreated = false;
    std::optional<StyleCardBankPosition> bankPosition;
    StyleCardTransition transition;
};

class StyleCardDeck
{
public:
    void setTransitionDurationSeconds (double seconds);
    [[nodiscard]] double transitionDurationSeconds() const noexcept;

    void addCard (StyleCard card);
    [[nodiscard]] const std::vector<StyleCard>& cards() const noexcept;

    bool setActive (const std::string& id, bool active, double nowSeconds);
    bool updateText (const std::string& id, std::string text);
    bool updateWeight (const std::string& id, float weight);
    bool updateActiveSlotWeight (int activeSlot, float weight);
    bool replaceCard (const std::string& targetId, StyleCard replacement, double nowSeconds);
    bool replaceCardFromExisting (const std::string& targetId, const std::string& sourceId, double nowSeconds);
    bool moveCardToActiveSlot (const std::string& cardId, int activeSlot, double nowSeconds);
    bool appendCardToBankColumn (const std::string& cardId, int column, double nowSeconds);
    bool switchBankCardPosition (const std::string& sourceCardId, const std::string& targetCardId, double nowSeconds);

    [[nodiscard]] PromptPortfolio effectivePromptPortfolio (double nowSeconds) const;
    [[nodiscard]] float effectiveWeightForCard (const StyleCard& card, double nowSeconds) const;
    [[nodiscard]] float activeSlotWeight (int activeSlot) const;
    [[nodiscard]] std::vector<StyleCard> bankCardsOrdered() const;

private:
    std::vector<StyleCard> cards_;
    std::array<float, 4> activeSlotWeights_ { 0.0f, 0.0f, 0.0f, 0.0f };
    double transitionDurationSeconds_ = 4.0;
};

[[nodiscard]] StyleCardDeck makeDefaultStyleCardDeck();

} // namespace mrt::plugin
