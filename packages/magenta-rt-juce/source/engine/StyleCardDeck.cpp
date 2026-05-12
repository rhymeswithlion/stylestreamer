#include "engine/StyleCardDeck.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>

namespace mrt::plugin
{
namespace
{

float clampWeight (float weight)
{
    return std::clamp (weight, 0.0f, 2.0f);
}

bool isTodoCardText (const std::string& text)
{
    return text.starts_with ("TODO:");
}

double clampDuration (double seconds)
{
    return std::clamp (seconds, 0.1, 30.0);
}

double transitionProgress (const StyleCardTransition& transition, double nowSeconds)
{
    if (! transition.inProgress || transition.durationSeconds <= 0.0)
        return 1.0;

    return std::clamp ((nowSeconds - transition.startSeconds) / transition.durationSeconds, 0.0, 1.0);
}

float colourSaturation (std::uint32_t argb)
{
    const auto red = static_cast<float> ((argb >> 16) & 0xff) / 255.0f;
    const auto green = static_cast<float> ((argb >> 8) & 0xff) / 255.0f;
    const auto blue = static_cast<float> (argb & 0xff) / 255.0f;
    const auto maxChannel = std::max ({ red, green, blue });
    const auto minChannel = std::min ({ red, green, blue });
    const auto lightness = (maxChannel + minChannel) * 0.5f;
    const auto chroma = maxChannel - minChannel;
    if (chroma == 0.0f)
        return 0.0f;

    return chroma / (1.0f - std::abs ((2.0f * lightness) - 1.0f));
}

std::uint32_t saturatedColourForCard (const StyleCard& card)
{
    constexpr float minimumSaturation = 0.45f;
    if (colourSaturation (card.colourArgb) >= minimumSaturation)
        return card.colourArgb;

    constexpr std::array<std::uint32_t, 10> palette {
        0xff0891b2,
        0xffdb2777,
        0xfff59e0b,
        0xff16a34a,
        0xff7c3aed,
        0xffdc2626,
        0xff2563eb,
        0xffea580c,
        0xff059669,
        0xffc026d3
    };
    const auto key = card.id + ":" + card.text;
    return palette[std::hash<std::string> {} (key) % palette.size()];
}

bool contributesToMix (const StyleCard& card)
{
    return card.active && ! isTodoCardText (card.text) && ! card.bankPosition.has_value();
}

std::size_t indexOfCard (const std::vector<StyleCard>& cards, const StyleCard& card)
{
    const auto* ptr = &card;
    for (std::size_t i = 0; i < cards.size(); ++i)
        if (&cards[i] == ptr)
            return i;

    return cards.size();
}

StyleCardTransition makeTransition (bool fromActive, bool toActive, double nowSeconds, double durationSeconds)
{
    return StyleCardTransition {
        .inProgress = true,
        .fromActive = fromActive,
        .toActive = toActive,
        .startSeconds = nowSeconds,
        .durationSeconds = durationSeconds
    };
}

} // namespace

void StyleCardDeck::setTransitionDurationSeconds (double seconds)
{
    transitionDurationSeconds_ = clampDuration (seconds);
}

double StyleCardDeck::transitionDurationSeconds() const noexcept
{
    return transitionDurationSeconds_;
}

void StyleCardDeck::addCard (StyleCard card)
{
    const auto newIndex = cards_.size();
    card.weight = clampWeight (card.weight);
    card.colourArgb = saturatedColourForCard (card);
    if (isTodoCardText (card.text))
        card.active = false;
    else if (card.bankPosition.has_value())
        card.active = true;
    else if (newIndex < activeSlotWeights_.size())
        activeSlotWeights_[newIndex] = card.weight;
    cards_.push_back (std::move (card));
}

const std::vector<StyleCard>& StyleCardDeck::cards() const noexcept
{
    return cards_;
}

bool StyleCardDeck::setActive (const std::string& id, bool active, double nowSeconds)
{
    auto card = std::find_if (cards_.begin(), cards_.end(),
        [&id] (const StyleCard& candidate) { return candidate.id == id; });
    if (card == cards_.end())
        return false;

    if (isTodoCardText (card->text))
    {
        card->active = false;
        card->transition = {};
        return true;
    }

    if (card->bankPosition.has_value())
    {
        card->active = true;
        return true;
    }

    if (card->active == active && ! card->transition.inProgress)
        return true;

    card->transition = makeTransition (card->active, active, nowSeconds, transitionDurationSeconds_);
    card->active = active;
    return true;
}

bool StyleCardDeck::updateText (const std::string& id, std::string text)
{
    auto card = std::find_if (cards_.begin(), cards_.end(),
        [&id] (const StyleCard& candidate) { return candidate.id == id; });
    if (card == cards_.end())
        return false;

    card->text = std::move (text);
    if (isTodoCardText (card->text))
        card->active = false;
    return true;
}

bool StyleCardDeck::updateWeight (const std::string& id, float weight)
{
    auto card = std::find_if (cards_.begin(), cards_.end(),
        [&id] (const StyleCard& candidate) { return candidate.id == id; });
    if (card == cards_.end())
        return false;

    card->weight = clampWeight (weight);
    const auto index = static_cast<std::size_t> (std::distance (cards_.begin(), card));
    if (! card->bankPosition.has_value() && index < activeSlotWeights_.size())
        activeSlotWeights_[index] = card->weight;
    return true;
}

bool StyleCardDeck::updateActiveSlotWeight (int activeSlot, float weight)
{
    if (activeSlot < 0 || activeSlot >= static_cast<int> (activeSlotWeights_.size()))
        return false;

    activeSlotWeights_[static_cast<std::size_t> (activeSlot)] = clampWeight (weight);
    return true;
}

bool StyleCardDeck::replaceCard (const std::string& targetId, StyleCard replacement, double nowSeconds)
{
    auto card = std::find_if (cards_.begin(), cards_.end(),
        [&targetId] (const StyleCard& candidate) { return candidate.id == targetId; });
    if (card == cards_.end())
        return false;

    const bool fromActive = card->active;
    replacement.weight = clampWeight (replacement.weight);
    replacement.colourArgb = saturatedColourForCard (replacement);
    if (isTodoCardText (replacement.text))
        replacement.active = false;
    else if (replacement.bankPosition.has_value())
        replacement.active = true;
    replacement.transition = makeTransition (
        fromActive,
        contributesToMix (replacement),
        nowSeconds,
        transitionDurationSeconds_);
    *card = std::move (replacement);
    return true;
}

bool StyleCardDeck::replaceCardFromExisting (const std::string& targetId, const std::string& sourceId, double nowSeconds)
{
    const auto target = std::find_if (cards_.begin(), cards_.end(),
        [&targetId] (const StyleCard& candidate) { return candidate.id == targetId; });
    const auto source = std::find_if (cards_.begin(), cards_.end(),
        [&sourceId] (const StyleCard& candidate) { return candidate.id == sourceId; });
    if (target == cards_.end() || source == cards_.end() || target == source)
        return false;
    if (! source->bankPosition.has_value())
        return false;

    const auto targetIndex = static_cast<std::size_t> (std::distance (cards_.begin(), target));
    const auto sourceIndex = static_cast<std::size_t> (std::distance (cards_.begin(), source));
    const auto sourceBankPosition = source->bankPosition;

    StyleCard movedToActive = *source;
    StyleCard movedToBank = *target;

    movedToActive.active = ! isTodoCardText (movedToActive.text);
    movedToActive.bankPosition.reset();
    movedToActive.transition = makeTransition (
        contributesToMix (*source),
        contributesToMix (movedToActive),
        nowSeconds,
        transitionDurationSeconds_);

    movedToBank.active = ! isTodoCardText (movedToBank.text);
    movedToBank.bankPosition = sourceBankPosition;
    movedToBank.transition = makeTransition (contributesToMix (*target), false, nowSeconds, transitionDurationSeconds_);

    cards_[targetIndex] = std::move (movedToActive);
    cards_[sourceIndex] = std::move (movedToBank);
    return true;
}

bool StyleCardDeck::moveCardToActiveSlot (const std::string& cardId, int activeSlot, double nowSeconds)
{
    if (activeSlot < 0 || activeSlot > static_cast<int> (cards_.size()))
        return false;

    auto card = std::find_if (cards_.begin(), cards_.end(),
        [&cardId] (const StyleCard& candidate) { return candidate.id == cardId; });
    if (card == cards_.end())
        return false;

    const auto targetIndex = static_cast<std::size_t> (activeSlot);
    const bool fromActive = contributesToMix (*card);
    StyleCard moved = std::move (*card);
    const auto sourceIndex = static_cast<std::size_t> (std::distance (cards_.begin(), card));
    cards_.erase (card);

    const auto adjustedTarget = sourceIndex < targetIndex ? targetIndex - 1 : targetIndex;
    auto insertAt = cards_.begin() + static_cast<std::ptrdiff_t> (std::min (adjustedTarget, cards_.size()));

    moved.active = ! isTodoCardText (moved.text);
    moved.bankPosition.reset();
    moved.transition = makeTransition (fromActive, contributesToMix (moved), nowSeconds, transitionDurationSeconds_);
    if (adjustedTarget < activeSlotWeights_.size() && activeSlotWeights_[adjustedTarget] == 0.0f)
        activeSlotWeights_[adjustedTarget] = moved.weight;
    cards_.insert (insertAt, std::move (moved));
    return true;
}

bool StyleCardDeck::appendCardToBankColumn (const std::string& cardId, int column, double nowSeconds)
{
    auto card = std::find_if (cards_.begin(), cards_.end(),
        [&cardId] (const StyleCard& candidate) { return candidate.id == cardId; });
    if (card == cards_.end())
        return false;

    int bottomRow = -1;
    for (const auto& candidate : cards_)
    {
        if (candidate.bankPosition.has_value() && candidate.bankPosition->column == column)
            bottomRow = std::max (bottomRow, candidate.bankPosition->row);
    }

    const bool fromActive = contributesToMix (*card);
    card->active = ! isTodoCardText (card->text);
    card->bankPosition = StyleCardBankPosition { .column = column, .row = bottomRow + 1 };
    card->transition = makeTransition (fromActive, false, nowSeconds, transitionDurationSeconds_);
    return true;
}

bool StyleCardDeck::switchBankCardPosition (
    const std::string& sourceCardId, const std::string& targetCardId, double nowSeconds)
{
    auto source = std::find_if (cards_.begin(), cards_.end(),
        [&sourceCardId] (const StyleCard& candidate) { return candidate.id == sourceCardId; });
    auto target = std::find_if (cards_.begin(), cards_.end(),
        [&targetCardId] (const StyleCard& candidate) { return candidate.id == targetCardId; });

    if (source == cards_.end() || target == cards_.end())
        return false;

    const bool sourceIsBank = source->bankPosition.has_value();
    const bool targetIsBank = target->bankPosition.has_value();
    if (sourceIsBank != targetIsBank)
        return sourceIsBank
            ? replaceCardFromExisting (targetCardId, sourceCardId, nowSeconds)
            : replaceCardFromExisting (sourceCardId, targetCardId, nowSeconds);

    if (! sourceIsBank && ! targetIsBank)
    {
        std::iter_swap (source, target);
        source->transition = makeTransition (source->active, source->active, nowSeconds, transitionDurationSeconds_);
        target->transition = makeTransition (target->active, target->active, nowSeconds, transitionDurationSeconds_);
        return true;
    }

    std::swap (source->bankPosition, target->bankPosition);
    source->transition = makeTransition (source->active, source->active, nowSeconds, transitionDurationSeconds_);
    target->transition = makeTransition (target->active, target->active, nowSeconds, transitionDurationSeconds_);
    return true;
}

float StyleCardDeck::effectiveWeightForCard (const StyleCard& card, double nowSeconds) const
{
    const auto cardIndex = indexOfCard (cards_, card);
    const auto slotWeight = cardIndex < activeSlotWeights_.size()
        ? activeSlotWeights_[cardIndex]
        : clampWeight (card.weight);

    if (! card.transition.inProgress)
        return contributesToMix (card) ? slotWeight : 0.0f;

    const auto progress = transitionProgress (card.transition, nowSeconds);
    const auto fromMultiplier = card.transition.fromActive ? 1.0 : 0.0;
    const auto toMultiplier = card.transition.toActive ? 1.0 : 0.0;
    const auto multiplier = fromMultiplier + ((toMultiplier - fromMultiplier) * progress);
    return static_cast<float> (slotWeight * multiplier);
}

PromptPortfolio StyleCardDeck::effectivePromptPortfolio (double nowSeconds) const
{
    PromptPortfolio portfolio (cards_.size());
    for (std::size_t i = 0; i < cards_.size(); ++i)
        portfolio.setSlot (i, cards_[i].text, effectiveWeightForCard (cards_[i], nowSeconds));
    return portfolio;
}

float StyleCardDeck::activeSlotWeight (int activeSlot) const
{
    if (activeSlot < 0 || activeSlot >= static_cast<int> (activeSlotWeights_.size()))
        return 0.0f;

    return activeSlotWeights_[static_cast<std::size_t> (activeSlot)];
}

std::vector<StyleCard> StyleCardDeck::bankCardsOrdered() const
{
    std::vector<StyleCard> bankCards;
    bankCards.reserve (cards_.size());
    for (const auto& card : cards_)
    {
        if (card.bankPosition.has_value())
            bankCards.push_back (card);
    }

    std::sort (bankCards.begin(), bankCards.end(), [] (const StyleCard& lhs, const StyleCard& rhs) {
        if (lhs.bankPosition->column != rhs.bankPosition->column)
            return lhs.bankPosition->column < rhs.bankPosition->column;
        return lhs.bankPosition->row < rhs.bankPosition->row;
    });

    return bankCards;
}

StyleCardDeck makeDefaultStyleCardDeck()
{
    constexpr int bankPromptRowsPerColumn = 7;
    constexpr std::array<std::uint32_t, 10> palette {
        0xff0891b2,
        0xffdb2777,
        0xfff59e0b,
        0xff16a34a,
        0xff7c3aed,
        0xffdc2626,
        0xff2563eb,
        0xffea580c,
        0xff059669,
        0xffc026d3
    };
    constexpr std::array<std::string_view, 5> imageKeys {
        "style-backgrounds/dubstep-drop.svg",
        "style-backgrounds/dnb-roller.svg",
        "style-backgrounds/festival-anthem.svg",
        "style-backgrounds/ambient-pad.svg",
        "style-backgrounds/techno-pulse.svg"
    };
    constexpr std::array<std::string_view, 65> bankPrompts {
        "deep house",
        "ambient synth pad",
        "lo-fi piano beats",
        "heavy metal guitar riff",
        "dream pop shimmer",
        "boom bap jazz loop",
        "orchestral film score",
        "dub reggae bassline",
        "acid house squelch",
        "trance supersaw lift",
        "drum and bass roller",
        "uk garage shuffle",
        "trap hi-hat cascade",
        "funk clavinet groove",
        "afrobeat polyrhythm",
        "bossa nova nylon guitar",
        "cinematic piano arpeggios",
        "synthwave night drive",
        "minimal dub chords",
        "progressive house build",
        "downtempo trip hop haze",
        "jungle amen pressure",
        "folk acoustic fingerpicking",
        "bluegrass banjo sprint",
        "chamber strings lament",
        "baroque harpsichord dance",
        "latin percussion jam",
        "marimba ensemble dance",
        "middle eastern oud melody",
        "indian tabla cycle",
        "japanese taiko thunder",
        "modal jazz midnight trio",
        "rhodes electric piano soul",
        "motown tambourine groove",
        "punk hardcore sprint",
        "emo guitar diary",
        "celtic fiddle reel",
        "sea shanty chorus",
        "gamelan bronze resonance",
        "kalimba sunset melody",
        "sparse piano and rain",
        "euphoric festival anthem",
        "dusty cassette beat tape",
        "haunted music box",
        "liquid drum and bass glide",
        "minimal piano nocturne",
        "sitar and tanpura dusk",
        "psytrance forest run",
        "dub techno chord fog",
        "electro swing horn shuffle",
        "french house filter groove",
        "neo soul rhodes pocket",
        "soul jazz organ combo",
        "boss battle orchestral drums",
        "anime opening rock sprint",
        "opera soprano storm",
        "desert blues guitar trance",
        "andes flute mountain air",
        "cumbia accordion bounce",
        "fado midnight lament",
        "mallet percussion ritual",
        "cloud rap vapor drift",
        "electronic gospel house lift",
        "hardstyle festival kick",
        "klezmer clarinet dance"
    };

    StyleCardDeck deck;
    deck.addCard ({ .id = "dubstep-drop",
        .text = "dubstep wobble bass",
        .weight = 0.88f,
        .active = true,
        .colourArgb = 0xff0891b2,
        .imageKey = "style-backgrounds/dubstep-drop.svg" });
    deck.addCard ({ .id = "dnb-roller",
        .text = "euphoric festival anthem",
        .weight = 0.83f,
        .active = true,
        .colourArgb = 0xffdb2777,
        .imageKey = "style-backgrounds/dnb-roller.svg" });
    deck.addCard ({ .id = "festival-anthem",
        .text = "lush string adagio",
        .weight = 0.78f,
        .active = true,
        .colourArgb = 0xfff59e0b,
        .imageKey = "style-backgrounds/festival-anthem.svg" });
    deck.addCard ({ .id = "warm-pad-texture",
        .text = "salsa brass hits",
        .weight = 0.59f,
        .active = true,
        .colourArgb = 0xff16a34a,
        .imageKey = "style-backgrounds/ambient-pad.svg" });

    for (std::size_t i = 0; i < bankPrompts.size(); ++i)
    {
        deck.addCard ({ .id = "bank-prompt-" + std::to_string (i + 1),
            .text = std::string (bankPrompts[i]),
            .weight = 0.50f,
            .active = false,
            .colourArgb = palette[i % palette.size()],
            .imageKey = std::string (imageKeys[i % imageKeys.size()]),
            .bankPosition = StyleCardBankPosition {
                .column = static_cast<int> (i / bankPromptRowsPerColumn),
                .row = static_cast<int> (i % bankPromptRowsPerColumn) } });
    }

    const int placeholderColumn = static_cast<int> ((bankPrompts.size() + bankPromptRowsPerColumn - 1) / bankPromptRowsPerColumn);
    deck.addCard ({ .id = "todo-favorite-instrument",
        .text = "TODO: your favorite instrument",
        .weight = 0.48f,
        .active = false,
        .colourArgb = 0xffdc2626,
        .imageKey = "style-backgrounds/festival-anthem.svg",
        .bankPosition = StyleCardBankPosition { .column = placeholderColumn, .row = 0 } });
    deck.addCard ({ .id = "todo-energetic-genre",
        .text = "TODO: an energetic genre",
        .weight = 0.46f,
        .active = false,
        .colourArgb = 0xff2563eb,
        .imageKey = "style-backgrounds/ambient-pad.svg",
        .bankPosition = StyleCardBankPosition { .column = placeholderColumn, .row = 1 } });
    deck.addCard ({ .id = "todo-rhythm-feel",
        .text = "TODO: a rhythm feel",
        .weight = 0.50f,
        .active = false,
        .colourArgb = 0xffea580c,
        .imageKey = "style-backgrounds/festival-anthem.svg",
        .bankPosition = StyleCardBankPosition { .column = placeholderColumn, .row = 2 } });
    return deck;
}

} // namespace mrt::plugin
