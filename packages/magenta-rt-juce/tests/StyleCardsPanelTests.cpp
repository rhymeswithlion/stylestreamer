#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "engine/StyleCardDeck.h"
#include "ui/StyleCardsPanel.h"

TEST_CASE ("StyleCardsPanel uses JUCE drag and drop built-ins", "[style-cards][ui]")
{
    mrt::plugin::StyleCardsPanel panel;

    CHECK (dynamic_cast<juce::DragAndDropContainer*> (&panel) != nullptr);
    CHECK (dynamic_cast<juce::DragAndDropTarget*> (&panel) != nullptr);
}

TEST_CASE ("StyleCardsPanel renders textured bank surface", "[style-cards][ui]")
{
    mrt::plugin::StyleCardsPanel panel;
    panel.setBounds (0, 0, 900, 540);
    panel.setDeck (mrt::plugin::StyleCardDeck {}, 0.0);

    juce::Image image (juce::Image::ARGB, panel.getWidth(), panel.getHeight(), true);
    juce::Graphics g (image);
    panel.paint (g);

    const auto leftFelt = image.getPixelAt (90, 350);
    const auto rightFelt = image.getPixelAt (450, 350);

    CHECK (leftFelt != rightFelt);
}

TEST_CASE ("StyleCardsPanel renders bank cards with soft live-card gradients", "[style-cards][ui]")
{
    mrt::plugin::StyleCardDeck deck;
    deck.addCard ({ .id = "live",
        .text = "live",
        .weight = 0.75f,
        .active = true,
        .colourArgb = 0xff0891b2 });
    deck.addCard ({ .id = "bank",
        .text = "x",
        .weight = 0.5f,
        .active = true,
        .colourArgb = 0xff0891b2,
        .bankPosition = mrt::plugin::StyleCardBankPosition { .column = 0, .row = 0 } });

    mrt::plugin::StyleCardsPanel panel;
    panel.setBounds (0, 0, 900, 540);
    panel.setDeck (deck, 0.0);

    juce::Image image (juce::Image::ARGB, panel.getWidth(), panel.getHeight(), true);
    juce::Graphics g (image);
    panel.paint (g);

    const auto liveTop = image.getPixelAt (45, 94);
    const auto liveBottom = image.getPixelAt (185, 150);
    const auto bankTop = image.getPixelAt (175, 286);
    const auto bankBottom = image.getPixelAt (45, 350);

    REQUIRE (liveTop != liveBottom);
    CHECK (bankTop != bankBottom);
}

TEST_CASE ("StyleCardsPanel computes native bank layout with exposed card tops", "[style-cards][ui]")
{
    mrt::plugin::StyleCardDeck deck;
    deck.addCard ({ .id = "a",
        .text = "lo-fi tape house",
        .weight = 0.88f,
        .active = false,
        .colourArgb = 0xff0891b2,
        .imageKey = "style-backgrounds/dubstep-drop.svg",
        .bankPosition = mrt::plugin::StyleCardBankPosition { .column = 0, .row = 0 } });
    deck.addCard ({ .id = "b",
        .text = "UK garage swing",
        .weight = 0.83f,
        .active = false,
        .colourArgb = 0xff7c3aed,
        .imageKey = "style-backgrounds/dnb-roller.svg",
        .bankPosition = mrt::plugin::StyleCardBankPosition { .column = 0, .row = 1 } });

    const auto layout = mrt::plugin::computeStyleCardBankLayout (deck, juce::Rectangle<int> (0, 0, 600, 260));

    REQUIRE (layout.cardBounds.size() == 2);
    REQUIRE (layout.placeholderBounds.has_value());
    CHECK (layout.cardBounds[1].getY() > layout.cardBounds[0].getY());
    CHECK (layout.cardBounds[1].getY() - layout.cardBounds[0].getY() >= layout.exposedTopHeight);
    CHECK (layout.placeholderBounds->getX() > layout.cardBounds[1].getX());
    CHECK (layout.placeholderBounds->getWidth() == layout.cardBounds[0].getWidth());
}

TEST_CASE ("StyleCardsPanel makes bank cards five percent wider while keeping fixed gaps", "[style-cards][ui]")
{
    mrt::plugin::StyleCardDeck deck;
    for (int column = 0; column < 5; ++column)
    {
        deck.addCard ({ .id = "bank-" + std::to_string (column),
            .text = "bank",
            .weight = 0.5f,
            .active = true,
            .bankPosition = mrt::plugin::StyleCardBankPosition { .column = column, .row = 0 } });
    }

    const juce::Rectangle<int> bankBounds (0, 0, 600, 260);
    const auto layout = mrt::plugin::computeStyleCardBankLayout (deck, bankBounds);
    const int previousFiveColumnWidth = (bankBounds.getWidth() - (12 * 4)) / 5;

    REQUIRE (layout.cardBounds.size() == 5);
    CHECK (layout.cardBounds[0].getWidth() == Catch::Approx (previousFiveColumnWidth * 1.05).margin (1.0));
    CHECK (layout.cardBounds[1].getX() - layout.cardBounds[0].getRight() == 12);
    CHECK (layout.cardBounds[4].getRight() > bankBounds.getRight());
}

TEST_CASE ("StyleCardsPanel allocates a bank area fifty percent taller in the main editor", "[style-cards][ui]")
{
    const auto previousBankBounds =
        mrt::plugin::computeStyleCardBankBoundsForPanel (juce::Rectangle<int> (0, 0, 900, 540));
    const auto tallerBankBounds =
        mrt::plugin::computeStyleCardBankBoundsForPanel (juce::Rectangle<int> (0, 0, 900, 686));

    CHECK (tallerBankBounds.getHeight() >= previousBankBounds.getHeight() * 3 / 2 - 2);
}

TEST_CASE ("StyleCardsPanel reports solitaire card drop targets", "[style-cards][ui]")
{
    mrt::plugin::StyleCardDeck deck;
    deck.addCard ({ .id = "source",
        .text = "source",
        .weight = 0.5f,
        .active = false,
        .bankPosition = mrt::plugin::StyleCardBankPosition { .column = 0, .row = 0 } });
    deck.addCard ({ .id = "target",
        .text = "target",
        .weight = 0.5f,
        .active = false,
        .bankPosition = mrt::plugin::StyleCardBankPosition { .column = 0, .row = 1 } });

    const juce::Rectangle<int> bankBounds (0, 0, 600, 260);
    const auto layout = mrt::plugin::computeStyleCardBankLayout (deck, bankBounds);
    REQUIRE (layout.cardBounds.size() == 2);

    const auto switchEvent = mrt::plugin::resolveStyleCardBankDropEvent (
        deck,
        "source",
        bankBounds,
        layout.cardBounds[1].getCentre());
    REQUIRE (switchEvent.has_value());
    CHECK (switchEvent->type == "bankSwitch");
    CHECK (switchEvent->cardId == "source");
    CHECK (switchEvent->targetId == "target");

    const auto appendEvent = mrt::plugin::resolveStyleCardBankDropEvent (
        deck,
        "source",
        bankBounds,
        layout.cardBounds[1].getBottomLeft().translated (0, layout.exposedTopHeight));
    REQUIRE (appendEvent.has_value());
    CHECK (appendEvent->type == "bankAppend");
    CHECK (appendEvent->cardId == "source");
    CHECK (appendEvent->targetColumn == 0);

    REQUIRE (layout.placeholderBounds.has_value());
    const auto createEvent = mrt::plugin::resolveStyleCardBankDropEvent (
        deck,
        "source",
        bankBounds,
        layout.placeholderBounds->getCentre());
    REQUIRE (createEvent.has_value());
    CHECK (createEvent->type == "create");
    CHECK (createEvent->targetColumn == 4);
}

TEST_CASE ("StyleCardsPanel drops bank cards onto empty active slots", "[style-cards][ui]")
{
    mrt::plugin::StyleCardDeck deck;
    deck.addCard ({ .id = "empty-slot",
        .text = "empty slot",
        .weight = 0.5f,
        .active = true,
        .bankPosition = mrt::plugin::StyleCardBankPosition { .column = 0, .row = 0 } });
    deck.addCard ({ .id = "active-a", .text = "active a", .weight = 0.5f, .active = true });
    deck.addCard ({ .id = "active-b", .text = "active b", .weight = 0.5f, .active = true });
    deck.addCard ({ .id = "active-c", .text = "active c", .weight = 0.5f, .active = true });
    deck.addCard ({ .id = "bank-source",
        .text = "bank source",
        .weight = 0.5f,
        .active = true,
        .bankPosition = mrt::plugin::StyleCardBankPosition { .column = 1, .row = 0 } });

    mrt::plugin::StyleCardsPanel panel;
    panel.setBounds (0, 0, 900, 540);
    panel.setDeck (deck, 0.0);

    std::optional<mrt::plugin::StyleCardEvent> event;
    panel.setEventHandler ([&event] (const mrt::plugin::StyleCardEvent& next) {
        event = next;
    });

    panel.itemDropped (juce::DragAndDropTarget::SourceDetails (
        juce::String ("style-card:bank-source"),
        &panel,
        juce::Point<int> (126, 151)));

    REQUIRE (event.has_value());
    CHECK (event->type == "activeSlot");
    CHECK (event->cardId == "bank-source");
    CHECK (event->targetSlot == 0);
}

TEST_CASE ("StyleCardsPanel dispatches inline text edits", "[style-cards][ui]")
{
    mrt::plugin::StyleCardDeck deck;
    deck.addCard ({ .id = "active", .text = "old text", .weight = 0.5f, .active = true });

    mrt::plugin::StyleCardsPanel panel;
    panel.setBounds (0, 0, 900, 540);
    panel.setDeck (deck, 0.0);

    std::optional<mrt::plugin::StyleCardEvent> event;
    panel.setEventHandler ([&event] (const mrt::plugin::StyleCardEvent& next) {
        event = next;
    });

    panel.beginEditingCardForTesting ("active");
    REQUIRE (panel.isEditingForTesting());
    REQUIRE (panel.editingTextEditorForTesting() != nullptr);
    panel.editingTextEditorForTesting()->setText ("new text", juce::dontSendNotification);
    panel.commitEditingForTesting();

    REQUIRE (event.has_value());
    CHECK (event->type == "text");
    CHECK (event->cardId == "active");
    CHECK (event->text == "new text");
    CHECK_FALSE (panel.isEditingForTesting());
}

TEST_CASE ("StyleCardsPanel computes horizontal slider states for active cards", "[style-cards][ui]")
{
    mrt::plugin::StyleCardDeck deck;
    deck.addCard ({ .id = "active",
        .text = "active",
        .weight = 0.72f,
        .active = true,
        .colourArgb = 0xff0891b2 });
    deck.addCard ({ .id = "inactive",
        .text = "inactive",
        .weight = 0.38f,
        .active = false,
        .colourArgb = 0xffdb2777 });
    deck.addCard ({ .id = "banked",
        .text = "banked",
        .weight = 0.91f,
        .active = true,
        .colourArgb = 0xfff59e0b,
        .bankPosition = mrt::plugin::StyleCardBankPosition { .column = 0, .row = 0 } });

    const auto states = mrt::plugin::computeStyleCardSliderStates (deck, juce::Rectangle<int> (0, 0, 900, 540));

    REQUIRE (states.size() == 2);
    CHECK (states[0].cardId == "active");
    CHECK (states[0].value == Catch::Approx (0.72f));
    CHECK (states[0].active);
    CHECK (states[0].tint.getSaturation() > 0.45f);
    CHECK (states[0].bounds.getWidth() > states[0].bounds.getHeight());
    CHECK (states[0].bounds.getY() > 120);

    CHECK (states[1].cardId == "inactive");
    CHECK (states[1].value == Catch::Approx (0.38f));
    CHECK_FALSE (states[1].active);
    CHECK (states[1].tint.getSaturation() < 0.10f);
}

TEST_CASE ("StyleCardsPanel exposes draggable weight sliders below live cards", "[style-cards][ui]")
{
    mrt::plugin::StyleCardDeck deck;
    deck.addCard ({ .id = "active",
        .text = "active",
        .weight = 0.72f,
        .active = true,
        .colourArgb = 0xff0891b2 });
    deck.addCard ({ .id = "inactive",
        .text = "inactive",
        .weight = 0.38f,
        .active = false,
        .colourArgb = 0xffdb2777 });

    mrt::plugin::StyleCardsPanel panel;
    panel.setBounds (0, 0, 900, 540);
    panel.setDeck (deck, 0.0);

    auto* activeSlider = panel.activeWeightSliderForTesting (0);
    REQUIRE (activeSlider != nullptr);
    CHECK (activeSlider->isVisible());
    CHECK (activeSlider->isEnabled());
    CHECK (activeSlider->getSliderStyle() == juce::Slider::LinearHorizontal);
    CHECK (activeSlider->getValue() == Catch::Approx (72.0));
    CHECK (activeSlider->findColour (juce::Slider::thumbColourId).getSaturation() > 0.45f);

    auto* inactiveSlider = panel.activeWeightSliderForTesting (1);
    REQUIRE (inactiveSlider != nullptr);
    CHECK (inactiveSlider->isVisible());
    CHECK (inactiveSlider->isEnabled());
    CHECK (inactiveSlider->getValue() == Catch::Approx (38.0));
    CHECK (inactiveSlider->findColour (juce::Slider::thumbColourId).getSaturation() < 0.10f);

    std::optional<mrt::plugin::StyleCardEvent> event;
    panel.setEventHandler ([&event] (const mrt::plugin::StyleCardEvent& next) {
        event = next;
    });
    activeSlider->setValue (64.0, juce::sendNotificationSync);

    REQUIRE (event.has_value());
    CHECK (event->type == "weight");
    CHECK (event->cardId == "active");
    CHECK (event->targetSlot == 0);
    CHECK (event->weightPercent == Catch::Approx (64.0));
}

TEST_CASE ("StyleCardsPanel keeps slider values with live slots when active cards move", "[style-cards][ui]")
{
    mrt::plugin::StyleCardDeck deck;
    deck.addCard ({ .id = "left", .text = "left", .weight = 0.25f, .active = true, .colourArgb = 0xff0891b2 });
    deck.addCard ({ .id = "right", .text = "right", .weight = 0.81f, .active = false, .colourArgb = 0xffdb2777 });
    REQUIRE (deck.updateActiveSlotWeight (0, 0.25f));
    REQUIRE (deck.updateActiveSlotWeight (1, 0.81f));

    REQUIRE (deck.switchBankCardPosition ("left", "right", 2.0));
    const auto states = mrt::plugin::computeStyleCardSliderStates (deck, juce::Rectangle<int> (0, 0, 900, 540));

    REQUIRE (states.size() == 2);
    CHECK (states[0].cardId == "right");
    CHECK (states[0].value == Catch::Approx (0.25f));
    CHECK_FALSE (states[0].active);

    CHECK (states[1].cardId == "left");
    CHECK (states[1].value == Catch::Approx (0.81f));
    CHECK (states[1].active);
}

TEST_CASE ("StyleCardsPanel shows transition dots during style ramps", "[style-cards][ui]")
{
    mrt::plugin::StyleCardDeck deck;
    deck.setTransitionDurationSeconds (4.0);
    deck.addCard ({ .id = "active", .text = "active", .weight = 0.5f, .active = true });
    REQUIRE (deck.setActive ("active", false, 10.0));

    mrt::plugin::StyleCardsPanel panel;
    panel.setDeck (deck, 12.0);
    CHECK (panel.isTransitionIndicatorVisibleForTesting());

    panel.setDeck (deck, 15.0);
    CHECK_FALSE (panel.isTransitionIndicatorVisibleForTesting());
}

TEST_CASE ("StyleCardsPanel advances one transition dot per second", "[style-cards][ui]")
{
    mrt::plugin::StyleCardDeck deck;
    deck.setTransitionDurationSeconds (4.0);
    deck.addCard ({ .id = "active", .text = "active", .weight = 0.5f, .active = true });
    REQUIRE (deck.setActive ("active", false, 10.0));

    mrt::plugin::StyleCardsPanel panel;

    panel.setDeck (deck, 10.2);
    CHECK (panel.activeTransitionDotIndexForTesting() == 0);

    panel.setDeck (deck, 11.2);
    CHECK (panel.activeTransitionDotIndexForTesting() == 1);

    panel.setDeck (deck, 12.2);
    CHECK (panel.activeTransitionDotIndexForTesting() == 2);

    panel.setDeck (deck, 13.2);
    CHECK (panel.activeTransitionDotIndexForTesting() == 3);

    panel.setDeck (deck, 14.0);
    CHECK (panel.activeTransitionDotIndexForTesting() == -1);
}
