#pragma once

#include "engine/StyleCardDeck.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace mrt::plugin
{

struct StyleCardEvent
{
    juce::String type;
    juce::String cardId;
    juce::String targetId;
    juce::String text;
    int targetColumn = -1;
    int targetSlot = -1;
    double weightPercent = 0.0;
};

struct StyleCardBankLayout
{
    std::vector<juce::Rectangle<int>> cardBounds;
    std::optional<juce::Rectangle<int>> placeholderBounds;
    int exposedTopHeight = 42;
};

struct StyleCardSliderState
{
    std::string cardId;
    juce::Rectangle<int> bounds;
    float value = 0.0f;
    bool active = true;
    juce::Colour tint;
};

[[nodiscard]] StyleCardBankLayout computeStyleCardBankLayout (
    const StyleCardDeck& deck,
    juce::Rectangle<int> bounds);

[[nodiscard]] juce::Rectangle<int> computeStyleCardBankBoundsForPanel (
    juce::Rectangle<int> bounds);

[[nodiscard]] std::vector<StyleCardSliderState> computeStyleCardSliderStates (
    const StyleCardDeck& deck,
    juce::Rectangle<int> bounds);

[[nodiscard]] std::optional<StyleCardEvent> resolveStyleCardBankDropEvent (
    const StyleCardDeck& deck,
    const std::string& sourceCardId,
    juce::Rectangle<int> bankBounds,
    juce::Point<int> dropPoint);

class StyleCardsPanel : public juce::Component,
                        public juce::DragAndDropContainer,
                        public juce::DragAndDropTarget,
                        private juce::ScrollBar::Listener
{
public:
    using EventHandler = std::function<void (const StyleCardEvent&)>;

    StyleCardsPanel();

    void setEventHandler (EventHandler handler);
    void setDeck (const StyleCardDeck& deck, double nowSeconds);
    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& event) override;
    void mouseDoubleClick (const juce::MouseEvent& event) override;
    void mouseDrag (const juce::MouseEvent& event) override;
    void mouseUp (const juce::MouseEvent& event) override;
    void mouseWheelMove (const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override;
    bool isInterestedInDragSource (const juce::DragAndDropTarget::SourceDetails& details) override;
    void itemDragMove (const juce::DragAndDropTarget::SourceDetails& details) override;
    void itemDragExit (const juce::DragAndDropTarget::SourceDetails& details) override;
    void itemDropped (const juce::DragAndDropTarget::SourceDetails& details) override;

    void beginEditingCardForTesting (const std::string& cardId);
    void commitEditingForTesting();
    [[nodiscard]] bool isEditingForTesting() const noexcept;
    [[nodiscard]] juce::TextEditor* editingTextEditorForTesting() noexcept;
    [[nodiscard]] bool isTransitionIndicatorVisibleForTesting() const;
    [[nodiscard]] int activeTransitionDotIndexForTesting() const;
    [[nodiscard]] juce::Slider* activeWeightSliderForTesting (int index) noexcept;

private:
    void scrollBarMoved (juce::ScrollBar* scrollBarThatHasMoved, double newRangeStart) override;
    void beginEditingCard (const std::string& cardId);
    void commitEditing();
    void cancelEditing();
    void updateBankScrollBar();
    void updateWeightSliders();

    StyleCardDeck deck_;
    EventHandler eventHandler_;
    std::string dragCardId_;
    std::string editingCardId_;
    juce::TextEditor editor_;
    std::array<juce::Slider, 4> weightSliders_;
    std::array<std::string, 4> weightSliderCardIds_;
    juce::ScrollBar bankScrollBar_ { false };
    juce::Image bankSurfaceImage_;
    juce::Point<int> dragStart_;
    int bankScrollOffset_ = 0;
    bool dragStarted_ = false;
    bool updatingWeightSliders_ = false;
    std::optional<StyleCardEvent> hoverDropEvent_;
    double nowSeconds_ = 0.0;
};

} // namespace mrt::plugin
