#include "ui/StyleCardsPanel.h"

#include "BinaryData.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace mrt::plugin
{
namespace
{

constexpr int visibleBankColumnCount = 5;
constexpr double bankCardWidthMultiplier = 1.05;
constexpr int cardHeight = 112;
constexpr int headerHeight = 54;
constexpr int activeAreaHeight = 158;
constexpr int outerPadding = 18;
constexpr int bankTopPadding = 12;
constexpr int bankScrollBarThickness = 14;
constexpr int dragStartDistance = 4;
constexpr const char* cardDragPrefix = "style-card:";

struct ActiveSlotBounds
{
    int index = 0;
    juce::Rectangle<int> bounds;
    std::optional<std::string> cardId;
};

[[nodiscard]] juce::Colour colourFromArgb (std::uint32_t argb)
{
    return juce::Colour (argb);
}

[[nodiscard]] juce::String dragDescriptionForCard (const std::string& cardId)
{
    return juce::String (cardDragPrefix) + juce::String (cardId);
}

[[nodiscard]] std::optional<std::string> cardIdFromDragDescription (const juce::var& description)
{
    const auto text = description.toString();
    if (! text.startsWith (cardDragPrefix))
        return std::nullopt;
    return text.fromFirstOccurrenceOf (cardDragPrefix, false, false).toStdString();
}

[[nodiscard]] int activeTransitionDotIndex (const StyleCardDeck& deck, double nowSeconds)
{
    for (const auto& card : deck.cards())
    {
        const auto& transition = card.transition;
        if (transition.inProgress
            && nowSeconds >= transition.startSeconds
            && nowSeconds < transition.startSeconds + transition.durationSeconds)
        {
            const auto elapsed = nowSeconds - transition.startSeconds;
            const auto secondsPerDot = transition.durationSeconds / 4.0;
            if (secondsPerDot <= 0.0)
                return -1;

            return std::clamp (static_cast<int> (elapsed / secondsPerDot), 0, 3);
        }
    }

    return -1;
}

[[nodiscard]] bool transitionIndicatorVisible (const StyleCardDeck& deck, double nowSeconds)
{
    return activeTransitionDotIndex (deck, nowSeconds) >= 0;
}

[[nodiscard]] int bankColumnCount (const StyleCardDeck& deck)
{
    int columnCount = visibleBankColumnCount;
    for (const auto& card : deck.bankCardsOrdered())
        if (card.bankPosition.has_value())
            columnCount = std::max (columnCount, card.bankPosition->column + 1);

    return columnCount;
}

[[nodiscard]] int bankColumnWidth (juce::Rectangle<int> bounds)
{
    constexpr int gap = 12;
    const int baseWidth =
        std::max (1, (bounds.getWidth() - (gap * (visibleBankColumnCount - 1))) / visibleBankColumnCount);
    return std::max (1, juce::roundToInt (static_cast<double> (baseWidth) * bankCardWidthMultiplier));
}

[[nodiscard]] int bankContentWidth (const StyleCardDeck& deck, juce::Rectangle<int> bounds)
{
    constexpr int gap = 12;
    const int columnCount = bankColumnCount (deck);
    return (columnCount * bankColumnWidth (bounds)) + (gap * std::max (0, columnCount - 1));
}

[[nodiscard]] std::vector<ActiveSlotBounds> activeSlots (
    const StyleCardDeck& deck,
    juce::Rectangle<int> bounds)
{
    std::vector<ActiveSlotBounds> result;
    auto content = bounds.reduced (outerPadding);
    content.removeFromTop (headerHeight);
    auto activeArea = content.removeFromTop (activeAreaHeight);
    const auto cards = deck.cards();
    const int activeWidth = std::max (1, activeArea.getWidth() / 4);

    result.reserve (4);
    for (int i = 0; i < 4; ++i)
    {
        const auto cardBounds = activeArea.removeFromLeft (activeWidth).reduced (6);
        std::optional<std::string> cardId;
        if (i < static_cast<int> (cards.size()))
        {
            const auto& card = cards[static_cast<std::size_t> (i)];
            if (! card.bankPosition.has_value())
                cardId = card.id;
        }

        result.push_back ({ .index = i, .bounds = cardBounds, .cardId = std::move (cardId) });
    }

    return result;
}

[[nodiscard]] std::vector<std::pair<std::string, juce::Rectangle<int>>> activeCardBounds (
    const StyleCardDeck& deck,
    juce::Rectangle<int> bounds)
{
    std::vector<std::pair<std::string, juce::Rectangle<int>>> result;
    const auto slots = activeSlots (deck, bounds);
    result.reserve (slots.size());
    for (const auto& slot : slots)
        if (slot.cardId.has_value())
            result.emplace_back (*slot.cardId, slot.bounds);

    return result;
}

[[nodiscard]] std::optional<std::string> cardAtPoint (
    const StyleCardDeck& deck,
    juce::Rectangle<int> bounds,
    int bankScrollOffset,
    juce::Point<int> point)
{
    const auto activeBounds = activeCardBounds (deck, bounds);
    for (const auto& [id, cardBounds] : activeBounds)
        if (cardBounds.contains (point))
            return id;

    const auto bankCards = deck.bankCardsOrdered();
    const auto bankLayout = computeStyleCardBankLayout (
        deck,
        computeStyleCardBankBoundsForPanel (bounds).translated (-bankScrollOffset, 0));
    for (std::size_t i = bankCards.size(); i > 0; --i)
    {
        const auto index = i - 1;
        if (index < bankLayout.cardBounds.size() && bankLayout.cardBounds[index].contains (point))
            return bankCards[index].id;
    }

    return std::nullopt;
}

[[nodiscard]] std::optional<juce::Rectangle<int>> boundsForCard (
    const StyleCardDeck& deck,
    juce::Rectangle<int> bounds,
    int bankScrollOffset,
    const std::string& cardId)
{
    for (const auto& [id, cardBounds] : activeCardBounds (deck, bounds))
        if (id == cardId)
            return cardBounds;

    const auto bankCards = deck.bankCardsOrdered();
    const auto bankLayout = computeStyleCardBankLayout (
        deck,
        computeStyleCardBankBoundsForPanel (bounds).translated (-bankScrollOffset, 0));
    for (std::size_t i = 0; i < bankCards.size() && i < bankLayout.cardBounds.size(); ++i)
        if (bankCards[i].id == cardId)
            return bankLayout.cardBounds[i];

    return std::nullopt;
}

[[nodiscard]] const StyleCard* findCard (const StyleCardDeck& deck, const std::string& cardId)
{
    const auto& cards = deck.cards();
    const auto card = std::find_if (cards.begin(), cards.end(), [&cardId] (const StyleCard& candidate) {
        return candidate.id == cardId;
    });
    return card == cards.end() ? nullptr : &*card;
}

[[nodiscard]] juce::Rectangle<int> activeCardBodyBounds (juce::Rectangle<int> slotBounds)
{
    return slotBounds.withTrimmedBottom (34);
}

[[nodiscard]] juce::Rectangle<int> sliderBoundsForSlot (juce::Rectangle<int> slotBounds)
{
    return slotBounds.withTrimmedTop (slotBounds.getHeight() - 30).reduced (12, 2);
}

void paintCard (
    juce::Graphics& g,
    const StyleCard& card,
    juce::Rectangle<int> cardBounds,
    bool bankCard)
{
    const auto colour = colourFromArgb (card.colourArgb);
    const auto corner = bankCard ? 12.0f : 18.0f;
    const auto base = card.active ? colour : juce::Colour (0xffcbd5e1);

    if (bankCard)
    {
        g.setGradientFill (juce::ColourGradient (
            base.brighter (0.18f), cardBounds.getTopLeft().toFloat(),
            base.withAlpha (card.active ? 0.35f : 0.72f), cardBounds.getBottomRight().toFloat(),
            false));
        g.fillRoundedRectangle (cardBounds.toFloat(), corner);
        g.setColour (juce::Colours::white.withAlpha (card.active ? 0.72f : 0.55f));
        g.drawRoundedRectangle (cardBounds.toFloat().reduced (0.5f), corner, 1.0f);
        g.setColour (card.active ? juce::Colours::white : juce::Colour (0xff64748b));
        g.drawFittedText (
            card.text,
            cardBounds.withHeight (42).reduced (8, 4),
            juce::Justification::centredLeft,
            1);
        return;
    }

    g.setGradientFill (juce::ColourGradient (
        base.brighter (0.18f), cardBounds.getTopLeft().toFloat(),
        base.withAlpha (card.active ? 0.35f : 0.72f), cardBounds.getBottomRight().toFloat(),
        false));
    g.fillRoundedRectangle (cardBounds.toFloat(), corner);
    g.setColour (juce::Colours::white.withAlpha (card.active ? 0.72f : 0.55f));
    g.drawRoundedRectangle (cardBounds.toFloat().reduced (0.5f), corner, 1.0f);
    g.setColour (card.active ? juce::Colours::white : juce::Colour (0xff64748b));
    g.setFont (juce::FontOptions (20.0f, juce::Font::bold));
    g.drawFittedText (
        card.text,
        cardBounds.withTrimmedBottom (44).reduced (12, 12),
        juce::Justification::topLeft,
        2,
        0.58f);

}

void paintActiveSlotPlaceholder (juce::Graphics& g, juce::Rectangle<int> bounds)
{
    g.setColour (juce::Colour (0xffe2e8f0).withAlpha (0.72f));
    g.fillRoundedRectangle (bounds.toFloat(), 18.0f);
    g.setColour (juce::Colour (0xff64748b).withAlpha (0.58f));
    g.drawRoundedRectangle (bounds.toFloat().reduced (0.5f), 18.0f, 1.5f);
    g.setColour (juce::Colour (0xff64748b));
    g.setFont (juce::FontOptions (14.0f, juce::Font::bold));
    g.drawFittedText (
        "Drop style here",
        bounds.reduced (12, 12),
        juce::Justification::centred,
        2);
}

void paintBankSurface (juce::Graphics& g, juce::Rectangle<int> bounds, const juce::Image& feltImage)
{
    constexpr auto corner = 12.0f;

    juce::Path clip;
    clip.addRoundedRectangle (bounds.toFloat(), corner);

    {
        juce::Graphics::ScopedSaveState state (g);
        g.reduceClipRegion (clip);

        if (feltImage.isNull())
        {
            g.setColour (juce::Colour (0xff276f35));
            g.fillRect (bounds);
        }
        else
        {
            g.drawImage (
                feltImage,
                bounds.toFloat(),
                juce::RectanglePlacement::fillDestination);
        }

        g.setColour (juce::Colour (0xff0f3d20).withAlpha (0.30f));
        g.fillRect (bounds);
    }

    g.setColour (juce::Colours::black.withAlpha (0.22f));
    g.drawRoundedRectangle (bounds.toFloat().reduced (0.5f), corner, 1.0f);
}

[[nodiscard]] juce::Image createDragImage (
    const StyleCardDeck& deck,
    juce::Rectangle<int> panelBounds,
    int bankScrollOffset,
    const std::string& cardId)
{
    const auto cardBounds = boundsForCard (deck, panelBounds, bankScrollOffset, cardId);
    const auto* card = findCard (deck, cardId);
    if (! cardBounds.has_value() || card == nullptr)
        return {};

    juce::Image image (juce::Image::ARGB, cardBounds->getWidth(), cardBounds->getHeight(), true);
    juce::Graphics g (image);
    paintCard (g, *card, image.getBounds(), card->bankPosition.has_value());
    image.multiplyAllAlphas (0.78f);
    return image;
}

[[nodiscard]] std::optional<StyleCardEvent> resolvePanelDropEvent (
    const StyleCardDeck& deck,
    juce::Rectangle<int> panelBounds,
    int bankScrollOffset,
    const std::string& sourceCardId,
    juce::Point<int> dropPoint)
{
    const auto activeBounds = activeCardBounds (deck, panelBounds);
    for (const auto& [targetId, targetBounds] : activeBounds)
    {
        if (targetId != sourceCardId && targetBounds.contains (dropPoint))
        {
            return StyleCardEvent {
                .type = "replace",
                .cardId = sourceCardId,
                .targetId = targetId
            };
        }
    }

    const auto slots = activeSlots (deck, panelBounds);
    for (const auto& slot : slots)
    {
        if (! slot.cardId.has_value() && slot.bounds.contains (dropPoint))
        {
            return StyleCardEvent {
                .type = "activeSlot",
                .cardId = sourceCardId,
                .targetSlot = slot.index
            };
        }
    }

    return resolveStyleCardBankDropEvent (
        deck,
        sourceCardId,
        computeStyleCardBankBoundsForPanel (panelBounds).translated (-bankScrollOffset, 0),
        dropPoint);
}

} // namespace

StyleCardBankLayout computeStyleCardBankLayout (
    const StyleCardDeck& deck,
    juce::Rectangle<int> bounds)
{
    StyleCardBankLayout layout;
    if (bounds.isEmpty())
        return layout;

    const auto bankCards = deck.bankCardsOrdered();
    const int columnCount = std::max (1, bankColumnCount (deck));
    const int gap = 12;
    const int columnWidth = bankColumnWidth (bounds);

    layout.cardBounds.reserve (bankCards.size());
    for (const auto& card : bankCards)
    {
        const auto position = card.bankPosition.value_or (StyleCardBankPosition {});
        const int column = std::clamp (position.column, 0, columnCount - 1);
        const int x = bounds.getX() + (column * (columnWidth + gap));
        const int y = bounds.getY() + (position.row * layout.exposedTopHeight);
        layout.cardBounds.push_back ({ x, y, columnWidth, cardHeight });
    }

    int placeholderRow = 0;
    for (const auto& card : bankCards)
    {
        if (card.bankPosition.has_value() && card.bankPosition->column == columnCount - 1)
            placeholderRow = std::max (placeholderRow, card.bankPosition->row + 1);
    }

    layout.placeholderBounds = juce::Rectangle<int> {
        bounds.getX() + ((columnCount - 1) * (columnWidth + gap)),
        bounds.getY() + (placeholderRow * layout.exposedTopHeight),
        columnWidth,
        cardHeight
    };

    return layout;
}

juce::Rectangle<int> computeStyleCardBankBoundsForPanel (juce::Rectangle<int> bounds)
{
    auto content = bounds.reduced (outerPadding);
    content.removeFromTop (headerHeight);
    content.removeFromTop (activeAreaHeight);
    return content.reduced (0, bankTopPadding).withTrimmedTop (32).withTrimmedBottom (bankScrollBarThickness + 4).reduced (12, 0);
}

std::vector<StyleCardSliderState> computeStyleCardSliderStates (
    const StyleCardDeck& deck,
    juce::Rectangle<int> bounds)
{
    std::vector<StyleCardSliderState> result;
    const auto slots = activeSlots (deck, bounds);
    result.reserve (slots.size());

    for (const auto& slot : slots)
    {
        if (! slot.cardId.has_value())
            continue;

        const auto* card = findCard (deck, *slot.cardId);
        if (card == nullptr)
            continue;

        result.push_back ({
            .cardId = card->id,
            .bounds = sliderBoundsForSlot (slot.bounds),
            .value = std::clamp (deck.activeSlotWeight (slot.index), 0.0f, 1.0f),
            .active = card->active,
            .tint = card->active ? colourFromArgb (card->colourArgb) : juce::Colour (0xff9a9a9a)
        });
    }

    return result;
}

std::optional<StyleCardEvent> resolveStyleCardBankDropEvent (
    const StyleCardDeck& deck,
    const std::string& sourceCardId,
    juce::Rectangle<int> bankBounds,
    juce::Point<int> dropPoint)
{
    const auto bankCards = deck.bankCardsOrdered();
    const auto layout = computeStyleCardBankLayout (deck, bankBounds);

    if (layout.placeholderBounds.has_value() && layout.placeholderBounds->contains (dropPoint))
        return StyleCardEvent {
            .type = "create",
            .cardId = sourceCardId,
            .targetColumn = bankColumnCount (deck) - 1
        };

    for (std::size_t i = bankCards.size(); i > 0; --i)
    {
        const auto index = i - 1;
        if (index >= layout.cardBounds.size())
            continue;

        if (layout.cardBounds[index].contains (dropPoint))
            return StyleCardEvent {
                .type = "bankSwitch",
                .cardId = sourceCardId,
                .targetId = bankCards[index].id
            };
    }

    const int gap = 12;
    const int columnCount = bankColumnCount (deck);
    const int columnWidth = bankColumnWidth (bankBounds);
    const int columnStride = columnWidth + gap;
    const int relativeX = dropPoint.getX() - bankBounds.getX();
    if (relativeX < 0 || relativeX >= bankContentWidth (deck, bankBounds))
        return std::nullopt;

    const int column = std::clamp (relativeX / columnStride, 0, columnCount - 1);
    int bottom = bankBounds.getY();
    bool hasColumn = false;
    for (std::size_t i = 0; i < bankCards.size() && i < layout.cardBounds.size(); ++i)
    {
        if (bankCards[i].bankPosition.has_value() && bankCards[i].bankPosition->column == column)
        {
            bottom = std::max (bottom, layout.cardBounds[i].getBottom());
            hasColumn = true;
        }
    }

    if (hasColumn && dropPoint.getY() >= bottom)
    {
        return StyleCardEvent {
            .type = "bankAppend",
            .cardId = sourceCardId,
            .targetColumn = column
        };
    }

    return std::nullopt;
}

StyleCardsPanel::StyleCardsPanel()
{
    juce::MemoryInputStream feltStream (BinaryData::green_felt_png, BinaryData::green_felt_pngSize, false);
    bankSurfaceImage_ = juce::ImageFileFormat::loadFrom (feltStream);

    bankScrollBar_.setAutoHide (true);
    bankScrollBar_.addListener (this);
    addAndMakeVisible (bankScrollBar_);

    for (std::size_t i = 0; i < weightSliders_.size(); ++i)
    {
        auto& slider = weightSliders_[i];
        slider.setSliderStyle (juce::Slider::LinearHorizontal);
        slider.setRange (0.0, 100.0, 1.0);
        slider.setNumDecimalPlacesToDisplay (0);
        slider.setTextValueSuffix ("%");
        slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 48, 20);
        slider.setVisible (false);
        slider.onValueChange = [this, i] {
            if (updatingWeightSliders_ || i >= weightSliderCardIds_.size() || weightSliderCardIds_[i].empty())
                return;

            if (eventHandler_)
            {
                eventHandler_ (StyleCardEvent {
                    .type = "weight",
                    .cardId = weightSliderCardIds_[i],
                    .targetSlot = static_cast<int> (i),
                    .weightPercent = weightSliders_[i].getValue()
                });
            }
        };
        addAndMakeVisible (slider);
    }

    editor_.setVisible (false);
    editor_.setMultiLine (false);
    editor_.setReturnKeyStartsNewLine (false);
    editor_.setSelectAllWhenFocused (true);
    editor_.setColour (juce::TextEditor::backgroundColourId, juce::Colours::white.withAlpha (0.92f));
    editor_.setColour (juce::TextEditor::outlineColourId, juce::Colour (0xff38bdf8));
    editor_.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colour (0xff0ea5e9));
    editor_.setColour (juce::TextEditor::textColourId, juce::Colour (0xff111827));
    editor_.onReturnKey = [this] { commitEditing(); };
    editor_.onEscapeKey = [this] { cancelEditing(); };
    editor_.onFocusLost = [this] {
        if (editor_.isVisible())
            commitEditing();
    };
    addChildComponent (editor_);
}

void StyleCardsPanel::setEventHandler (EventHandler handler)
{
    eventHandler_ = std::move (handler);
}

void StyleCardsPanel::setDeck (const StyleCardDeck& deck, double nowSeconds)
{
    deck_ = deck;
    nowSeconds_ = nowSeconds;
    updateBankScrollBar();
    updateWeightSliders();
    repaint();
}

void StyleCardsPanel::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::transparentBlack);

    auto dashboard = getLocalBounds().reduced (4);
    g.setColour (juce::Colour (0xfff8fafc));
    g.fillRoundedRectangle (dashboard.toFloat(), 18.0f);
    g.setColour (juce::Colour (0xffd7dee8));
    g.drawRoundedRectangle (dashboard.toFloat().reduced (0.5f), 18.0f, 1.0f);

    auto bounds = getLocalBounds().reduced (outerPadding);
    auto header = bounds.removeFromTop (headerHeight);
    g.setColour (juce::Colour (0xff64748b));
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.drawText (
        juce::String ("STYLESTREAMER v") + VERSION,
        header.removeFromTop (16),
        juce::Justification::centredLeft);
    g.setColour (juce::Colour (0xff111827));
    g.setFont (juce::FontOptions (25.0f, juce::Font::bold));
    auto titleRow = header.removeFromTop (32);
    g.drawText ("Live Style Mix", titleRow, juce::Justification::centredLeft);

    if (transitionIndicatorVisible (deck_, nowSeconds_))
    {
        const auto dotBase = titleRow.withLeft (titleRow.getX() + 170).withWidth (78).withHeight (20).reduced (0, 5);
        const int activeDot = activeTransitionDotIndex (deck_, nowSeconds_);
        for (int i = 0; i < 4; ++i)
        {
            const auto alpha = i == activeDot ? 0.92f : 0.28f;
            g.setColour (juce::Colour (0xff0ea5e9).withAlpha (alpha));
            g.fillEllipse (static_cast<float> (dotBase.getX() + (i * 18)),
                static_cast<float> (dotBase.getY()),
                8.0f,
                8.0f);
        }
    }

    bounds.removeFromTop (activeAreaHeight);
    auto bankArea = bounds.reduced (0, bankTopPadding);

    for (const auto& slot : activeSlots (deck_, getLocalBounds()))
    {
        if (slot.cardId.has_value())
        {
            if (const auto* card = findCard (deck_, *slot.cardId))
                paintCard (g, *card, activeCardBodyBounds (slot.bounds), false);
        }
        else
        {
            paintActiveSlotPlaceholder (g, activeCardBodyBounds (slot.bounds));
        }
    }

    auto bankSurface = bankArea.reduced (0, 4);
    paintBankSurface (g, bankSurface, bankSurfaceImage_);
    auto bankHeader = bankSurface.removeFromTop (28);
    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    g.drawText ("Card Bank", bankHeader.reduced (12, 0), juce::Justification::centredLeft);
    g.setColour (juce::Colours::white.withAlpha (0.76f));
    g.setFont (juce::FontOptions (12.0f));
    g.drawText (
        "Drag any exposed card top into an active space",
        bankHeader.reduced (12, 0),
        juce::Justification::centredRight);

    const auto bankCards = deck_.bankCardsOrdered();
    const auto visibleBankBounds = computeStyleCardBankBoundsForPanel (getLocalBounds());
    const auto bankLayout = computeStyleCardBankLayout (deck_, visibleBankBounds.translated (-bankScrollOffset_, 0));
    {
        juce::Graphics::ScopedSaveState bankClip (g);
        g.reduceClipRegion (visibleBankBounds);
        for (std::size_t i = 0; i < bankCards.size() && i < bankLayout.cardBounds.size(); ++i)
        {
            const auto& card = bankCards[i];
            const auto cardBounds = bankLayout.cardBounds[i];
            paintCard (g, card, cardBounds, true);
        }

        if (bankLayout.placeholderBounds.has_value())
        {
            const auto placeholder = *bankLayout.placeholderBounds;
            g.setColour (juce::Colours::white.withAlpha (0.12f));
            g.fillRoundedRectangle (placeholder.toFloat(), 12.0f);
            g.setColour (juce::Colours::white.withAlpha (0.72f));
            g.drawRoundedRectangle (placeholder.toFloat().reduced (1.0f), 12.0f, 1.5f);
            g.setFont (juce::FontOptions (26.0f, juce::Font::bold));
            g.drawText ("+", placeholder.withHeight (52), juce::Justification::centred);
        }
    }

    if (hoverDropEvent_.has_value())
    {
        std::optional<juce::Rectangle<int>> highlight;
        if (hoverDropEvent_->type == "replace")
            highlight = boundsForCard (deck_, getLocalBounds(), bankScrollOffset_, hoverDropEvent_->targetId.toStdString());
        else if (hoverDropEvent_->type == "activeSlot")
        {
            const auto slots = activeSlots (deck_, getLocalBounds());
            const auto targetSlot = hoverDropEvent_->targetSlot;
            const auto slot = std::find_if (slots.begin(), slots.end(), [targetSlot] (const ActiveSlotBounds& candidate) {
                return candidate.index == targetSlot;
            });
            if (slot != slots.end())
                highlight = slot->bounds;
        }
        else if (hoverDropEvent_->type == "bankSwitch")
            highlight = boundsForCard (deck_, getLocalBounds(), bankScrollOffset_, hoverDropEvent_->targetId.toStdString());
        else if (hoverDropEvent_->type == "bankAppend")
        {
            const int column = std::max (0, hoverDropEvent_->targetColumn);
            const auto bankBounds = computeStyleCardBankBoundsForPanel (getLocalBounds()).translated (-bankScrollOffset_, 0);
            const int gap = 12;
            const int columnWidth = bankColumnWidth (bankBounds);
            int bottom = bankBounds.getY();
            for (std::size_t i = 0; i < bankCards.size() && i < bankLayout.cardBounds.size(); ++i)
                if (bankCards[i].bankPosition.has_value() && bankCards[i].bankPosition->column == column)
                    bottom = std::max (bottom, bankLayout.cardBounds[i].getBottom());
            highlight = juce::Rectangle<int> (
                bankBounds.getX() + (column * (columnWidth + gap)),
                bottom + 4,
                columnWidth,
                bankLayout.exposedTopHeight);
        }

        if (highlight.has_value())
        {
            g.setColour (juce::Colour (0xffffd54f).withAlpha (0.34f));
            g.fillRoundedRectangle (highlight->toFloat(), 12.0f);
            g.setColour (juce::Colour (0xffffd54f));
            g.drawRoundedRectangle (highlight->toFloat().reduced (1.0f), 12.0f, 2.0f);
        }
    }
}

void StyleCardsPanel::resized()
{
    updateBankScrollBar();
    updateWeightSliders();
    if (! editingCardId_.empty())
        beginEditingCard (editingCardId_);
}

void StyleCardsPanel::mouseDown (const juce::MouseEvent& event)
{
    dragStart_ = event.getPosition();
    dragStarted_ = false;
    hoverDropEvent_.reset();
    if (const auto cardId = cardAtPoint (deck_, getLocalBounds(), bankScrollOffset_, dragStart_))
        dragCardId_ = *cardId;
    else
        dragCardId_.clear();
}

void StyleCardsPanel::mouseDoubleClick (const juce::MouseEvent& event)
{
    if (const auto cardId = cardAtPoint (deck_, getLocalBounds(), bankScrollOffset_, event.getPosition()))
    {
        dragCardId_.clear();
        beginEditingCard (*cardId);
    }
}

void StyleCardsPanel::mouseDrag (const juce::MouseEvent& event)
{
    if (dragCardId_.empty() || dragStarted_ || event.getDistanceFromDragStart() < dragStartDistance)
        return;

    dragStarted_ = true;
    const auto dragImage = createDragImage (deck_, getLocalBounds(), bankScrollOffset_, dragCardId_);
    juce::Point<int> offset { dragImage.getWidth() / 2, dragImage.getHeight() / 2 };
    startDragging (
        dragDescriptionForCard (dragCardId_),
        this,
        juce::ScaledImage (dragImage),
        false,
        dragImage.isValid() ? &offset : nullptr,
        &event.source);
}

void StyleCardsPanel::mouseUp (const juce::MouseEvent& event)
{
    if (! eventHandler_)
        return;

    const auto dropPoint = event.getPosition();
    if (dragCardId_.empty())
    {
        const auto bankLayout = computeStyleCardBankLayout (
            deck_,
            computeStyleCardBankBoundsForPanel (getLocalBounds()).translated (-bankScrollOffset_, 0));
        if (bankLayout.placeholderBounds.has_value()
            && bankLayout.placeholderBounds->contains (dropPoint)
            && dragStart_.getDistanceFrom (dropPoint) < 4.0f)
        {
            eventHandler_ (StyleCardEvent { .type = "create", .targetColumn = bankColumnCount (deck_) - 1 });
        }
        return;
    }

    if (dragStarted_)
    {
        dragStarted_ = false;
        dragCardId_.clear();
        hoverDropEvent_.reset();
        repaint();
        return;
    }

    if (dragStart_.getDistanceFrom (dropPoint) < 4.0f)
    {
        eventHandler_ (StyleCardEvent { .type = "toggle", .cardId = dragCardId_ });
    }

    dragCardId_.clear();
}

void StyleCardsPanel::mouseWheelMove (const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    const auto bankSurface = computeStyleCardBankBoundsForPanel (getLocalBounds()).expanded (12, 36);
    if (! bankSurface.contains (event.getPosition()) || bankScrollBar_.getMaximumRangeLimit() <= bankScrollBar_.getCurrentRangeSize())
        return;

    const auto delta = static_cast<double> (bankColumnWidth (computeStyleCardBankBoundsForPanel (getLocalBounds()))) * 0.35;
    const auto wheelSteps = std::abs (wheel.deltaX) > std::abs (wheel.deltaY) ? -wheel.deltaX : wheel.deltaY;
    bankScrollBar_.setCurrentRangeStart (bankScrollBar_.getCurrentRangeStart() - (static_cast<double> (wheelSteps) * delta));
}

bool StyleCardsPanel::isInterestedInDragSource (const juce::DragAndDropTarget::SourceDetails& details)
{
    return cardIdFromDragDescription (details.description).has_value();
}

void StyleCardsPanel::itemDragMove (const juce::DragAndDropTarget::SourceDetails& details)
{
    const auto cardId = cardIdFromDragDescription (details.description);
    if (! cardId.has_value())
        return;

    hoverDropEvent_ = resolvePanelDropEvent (deck_, getLocalBounds(), bankScrollOffset_, *cardId, details.localPosition);
    repaint();
}

void StyleCardsPanel::itemDragExit (const juce::DragAndDropTarget::SourceDetails&)
{
    hoverDropEvent_.reset();
    repaint();
}

void StyleCardsPanel::itemDropped (const juce::DragAndDropTarget::SourceDetails& details)
{
    const auto cardId = cardIdFromDragDescription (details.description);
    if (! cardId.has_value())
        return;

    auto event = resolvePanelDropEvent (deck_, getLocalBounds(), bankScrollOffset_, *cardId, details.localPosition);
    hoverDropEvent_.reset();
    dragStarted_ = false;
    dragCardId_.clear();

    if (event.has_value() && eventHandler_)
        eventHandler_ (*event);

    repaint();
}

void StyleCardsPanel::beginEditingCardForTesting (const std::string& cardId)
{
    beginEditingCard (cardId);
}

void StyleCardsPanel::commitEditingForTesting()
{
    commitEditing();
}

bool StyleCardsPanel::isEditingForTesting() const noexcept
{
    return editor_.isVisible();
}

juce::TextEditor* StyleCardsPanel::editingTextEditorForTesting() noexcept
{
    return editor_.isVisible() ? &editor_ : nullptr;
}

bool StyleCardsPanel::isTransitionIndicatorVisibleForTesting() const
{
    return transitionIndicatorVisible (deck_, nowSeconds_);
}

int StyleCardsPanel::activeTransitionDotIndexForTesting() const
{
    return activeTransitionDotIndex (deck_, nowSeconds_);
}

juce::Slider* StyleCardsPanel::activeWeightSliderForTesting (int index) noexcept
{
    if (index < 0 || index >= static_cast<int> (weightSliders_.size()))
        return nullptr;

    return &weightSliders_[static_cast<std::size_t> (index)];
}

void StyleCardsPanel::scrollBarMoved (juce::ScrollBar* scrollBarThatHasMoved, double newRangeStart)
{
    if (scrollBarThatHasMoved != &bankScrollBar_)
        return;

    bankScrollOffset_ = juce::roundToInt (newRangeStart);
    repaint();
}

void StyleCardsPanel::updateBankScrollBar()
{
    const auto bankBounds = computeStyleCardBankBoundsForPanel (getLocalBounds());
    if (bankBounds.isEmpty())
    {
        bankScrollBar_.setVisible (false);
        bankScrollOffset_ = 0;
        return;
    }

    const auto contentWidth = bankContentWidth (deck_, bankBounds);
    const auto visibleWidth = bankBounds.getWidth();
    bankScrollOffset_ = std::clamp (bankScrollOffset_, 0, std::max (0, contentWidth - visibleWidth));

    bankScrollBar_.setBounds (
        bankBounds.getX(),
        bankBounds.getBottom() + 4,
        visibleWidth,
        bankScrollBarThickness);
    bankScrollBar_.setRangeLimits (0.0, static_cast<double> (contentWidth), juce::dontSendNotification);
    bankScrollBar_.setCurrentRange (
        static_cast<double> (bankScrollOffset_),
        static_cast<double> (visibleWidth),
        juce::dontSendNotification);
    bankScrollBar_.setVisible (contentWidth > visibleWidth);
    bankScrollBar_.toFront (false);
}

void StyleCardsPanel::updateWeightSliders()
{
    updatingWeightSliders_ = true;
    for (std::size_t i = 0; i < weightSliders_.size(); ++i)
    {
        weightSliderCardIds_[i].clear();
        weightSliders_[i].setVisible (false);
    }

    const auto states = computeStyleCardSliderStates (deck_, getLocalBounds());
    for (std::size_t i = 0; i < states.size() && i < weightSliders_.size(); ++i)
    {
        const auto& state = states[i];
        auto& slider = weightSliders_[i];
        const auto tint = state.active ? state.tint : juce::Colour (0xff8a8a8a);

        weightSliderCardIds_[i] = state.cardId;
        slider.setBounds (state.bounds);
        slider.setEnabled (true);
        slider.setVisible (true);
        slider.setColour (juce::Slider::thumbColourId, tint);
        slider.setColour (juce::Slider::trackColourId, tint.withAlpha (state.active ? 0.92f : 0.58f));
        slider.setColour (juce::Slider::backgroundColourId, juce::Colours::black.withAlpha (0.18f));
        slider.setColour (juce::Slider::textBoxTextColourId, state.active ? juce::Colour (0xff111827) : juce::Colour (0xff737373));
        slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::white.withAlpha (0.72f));
        slider.setColour (juce::Slider::textBoxOutlineColourId, tint.withAlpha (0.55f));
        slider.setValue (static_cast<double> (state.value * 100.0f), juce::dontSendNotification);
    }

    updatingWeightSliders_ = false;
}

void StyleCardsPanel::beginEditingCard (const std::string& cardId)
{
    const auto* card = findCard (deck_, cardId);
    const auto cardBounds = boundsForCard (deck_, getLocalBounds(), bankScrollOffset_, cardId);
    if (card == nullptr || ! cardBounds.has_value())
        return;

    editingCardId_ = cardId;
    const auto editorBounds = cardBounds->withHeight (38).reduced (8, 6);
    editor_.setBounds (editorBounds);
    editor_.setText (card->text, juce::dontSendNotification);
    editor_.setVisible (true);
    editor_.toFront (false);
    editor_.grabKeyboardFocus();
    editor_.selectAll();
}

void StyleCardsPanel::commitEditing()
{
    if (editingCardId_.empty())
        return;

    const auto cardId = editingCardId_;
    const auto text = editor_.getText();
    editingCardId_.clear();
    editor_.setVisible (false);

    if (eventHandler_)
        eventHandler_ (StyleCardEvent { .type = "text", .cardId = cardId, .text = text });

    repaint();
}

void StyleCardsPanel::cancelEditing()
{
    editingCardId_.clear();
    editor_.setVisible (false);
    repaint();
}

} // namespace mrt::plugin
