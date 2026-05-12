#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "engine/StyleCardDeck.h"
#include "ui/StyleCardsPanel.h"

namespace
{

[[nodiscard]] float colourSaturation (std::uint32_t argb)
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

} // namespace

TEST_CASE ("StyleCardDeck excludes inactive cards without changing displayed weights", "[style-cards]")
{
    mrt::plugin::StyleCardDeck deck;
    deck.addCard ({ .id = "a", .text = "dubstep", .weight = 0.88f, .active = true });
    deck.addCard ({ .id = "b", .text = "festival", .weight = 0.78f, .active = true });

    deck.setActive ("b", false, 0.0);

    const auto& cards = deck.cards();
    REQUIRE (cards.size() == 2);
    CHECK (cards[1].weight == Catch::Approx (0.78f));
    CHECK_FALSE (cards[1].active);

    const auto effective = deck.effectivePromptPortfolio (5.0);
    const auto prompts = effective.normalizedActivePrompts();
    REQUIRE (prompts.size() == 1);
    CHECK (prompts[0].text == "dubstep");
    CHECK (prompts[0].weight == Catch::Approx (0.88f));
}

TEST_CASE ("StyleCardDeck ramps pending card contribution over time", "[style-cards]")
{
    mrt::plugin::StyleCardDeck deck;
    deck.setTransitionDurationSeconds (4.0);
    deck.addCard ({ .id = "a", .text = "dnb", .weight = 0.80f, .active = true });

    deck.setActive ("a", false, 10.0);

    auto half = deck.effectivePromptPortfolio (12.0).activeSlotsOrdered();
    REQUIRE (half.size() == 1);
    CHECK (half[0].weight == Catch::Approx (0.40f));

    auto done = deck.effectivePromptPortfolio (14.0).activeSlotsOrdered();
    CHECK (done.empty());
}

TEST_CASE ("StyleCardDeck appends bank cards below the chosen column and keeps them active", "[style-cards]")
{
    mrt::plugin::StyleCardDeck deck;
    deck.addCard ({ .id = "a", .text = "house", .weight = 0.5f, .active = false, .bankPosition = mrt::plugin::StyleCardBankPosition { .column = 2, .row = 0 } });
    deck.addCard ({ .id = "b", .text = "garage", .weight = 0.6f, .active = false, .bankPosition = mrt::plugin::StyleCardBankPosition { .column = 2, .row = 3 } });
    deck.addCard ({ .id = "c", .text = "techno", .weight = 0.7f, .active = true });

    REQUIRE (deck.appendCardToBankColumn ("c", 2, 8.0));

    const auto& cards = deck.cards();
    REQUIRE (cards[2].bankPosition.has_value());
    CHECK (cards[2].active);
    CHECK (cards[2].bankPosition->column == 2);
    CHECK (cards[2].bankPosition->row == 4);
    CHECK (cards[2].transition.inProgress);
    CHECK (cards[2].transition.fromActive);
    CHECK_FALSE (cards[2].transition.toActive);
}

TEST_CASE ("StyleCardDeck switches two bank card positions", "[style-cards]")
{
    mrt::plugin::StyleCardDeck deck;
    deck.addCard ({ .id = "a", .text = "house", .active = false, .bankPosition = mrt::plugin::StyleCardBankPosition { .column = 0, .row = 2 } });
    deck.addCard ({ .id = "b", .text = "garage", .active = false, .bankPosition = mrt::plugin::StyleCardBankPosition { .column = 3, .row = 1 } });

    REQUIRE (deck.switchBankCardPosition ("a", "b", 3.5));

    const auto& cards = deck.cards();
    REQUIRE (cards[0].bankPosition.has_value());
    REQUIRE (cards[1].bankPosition.has_value());
    CHECK (cards[0].bankPosition->column == 3);
    CHECK (cards[0].bankPosition->row == 1);
    CHECK (cards[1].bankPosition->column == 0);
    CHECK (cards[1].bankPosition->row == 2);
    CHECK (cards[0].transition.inProgress);
    CHECK (cards[1].transition.inProgress);
}

TEST_CASE ("StyleCardDeck switches an active card with an existing bank card", "[style-cards]")
{
    mrt::plugin::StyleCardDeck deck;
    deck.addCard ({ .id = "active", .text = "house", .active = true });
    deck.addCard ({ .id = "bank",
        .text = "garage",
        .active = false,
        .bankPosition = mrt::plugin::StyleCardBankPosition { .column = 3, .row = 1 } });

    REQUIRE (deck.switchBankCardPosition ("active", "bank", 4.0));

    const auto& cards = deck.cards();
    REQUIRE (cards.size() == 2);
    CHECK (cards[0].id == "bank");
    CHECK (cards[0].active);
    CHECK_FALSE (cards[0].bankPosition.has_value());
    CHECK (cards[0].transition.inProgress);
    CHECK_FALSE (cards[0].transition.fromActive);
    CHECK (cards[0].transition.toActive);

    CHECK (cards[1].id == "active");
    CHECK (cards[1].active);
    REQUIRE (cards[1].bankPosition.has_value());
    CHECK (cards[1].bankPosition->column == 3);
    CHECK (cards[1].bankPosition->row == 1);
    CHECK (cards[1].transition.inProgress);
    CHECK (cards[1].transition.fromActive);
    CHECK_FALSE (cards[1].transition.toActive);
}

TEST_CASE ("StyleCardDeck switches two active card slots", "[style-cards]")
{
    mrt::plugin::StyleCardDeck deck;
    deck.addCard ({ .id = "left", .text = "house", .weight = 0.25f, .active = true });
    deck.addCard ({ .id = "right", .text = "garage", .weight = 0.81f, .active = true });

    REQUIRE (deck.switchBankCardPosition ("left", "right", 5.0));

    const auto& cards = deck.cards();
    REQUIRE (cards.size() == 2);
    CHECK (cards[0].id == "right");
    CHECK (cards[0].active);
    CHECK_FALSE (cards[0].bankPosition.has_value());
    CHECK (cards[0].transition.inProgress);
    CHECK (cards[0].transition.fromActive);
    CHECK (cards[0].transition.toActive);

    CHECK (cards[1].id == "left");
    CHECK (cards[1].active);
    CHECK_FALSE (cards[1].bankPosition.has_value());
    CHECK (cards[1].transition.inProgress);
    CHECK (cards[1].transition.fromActive);
    CHECK (cards[1].transition.toActive);

    CHECK (deck.activeSlotWeight (0) == Catch::Approx (0.25f));
    CHECK (deck.activeSlotWeight (1) == Catch::Approx (0.81f));
    const auto activeSlots = deck.effectivePromptPortfolio (9.0).activeSlotsOrdered();
    REQUIRE (activeSlots.size() == 2);
    CHECK (activeSlots[0].text == "garage");
    CHECK (activeSlots[0].weight == Catch::Approx (0.25f));
    CHECK (activeSlots[1].text == "house");
    CHECK (activeSlots[1].weight == Catch::Approx (0.81f));
}

TEST_CASE ("StyleCardDeck slider weights belong to active slots rather than cards", "[style-cards]")
{
    mrt::plugin::StyleCardDeck deck;
    deck.addCard ({ .id = "slot-a", .text = "slot a", .weight = 0.20f, .active = true });
    deck.addCard ({ .id = "slot-b", .text = "slot b", .weight = 0.80f, .active = true });
    REQUIRE (deck.updateActiveSlotWeight (0, 0.33f));
    REQUIRE (deck.updateActiveSlotWeight (1, 0.77f));

    REQUIRE (deck.switchBankCardPosition ("slot-a", "slot-b", 1.0));

    const auto activeSlots = deck.effectivePromptPortfolio (5.0).activeSlotsOrdered();
    REQUIRE (activeSlots.size() == 2);
    CHECK (activeSlots[0].text == "slot b");
    CHECK (activeSlots[0].weight == Catch::Approx (0.33f));
    CHECK (activeSlots[1].text == "slot a");
    CHECK (activeSlots[1].weight == Catch::Approx (0.77f));
}

TEST_CASE ("StyleCardDeck orders bank cards by column then row", "[style-cards]")
{
    mrt::plugin::StyleCardDeck deck;
    deck.addCard ({ .id = "z", .text = "late", .active = false, .bankPosition = mrt::plugin::StyleCardBankPosition { .column = 2, .row = 0 } });
    deck.addCard ({ .id = "a", .text = "first", .active = false, .bankPosition = mrt::plugin::StyleCardBankPosition { .column = 0, .row = 1 } });
    deck.addCard ({ .id = "b", .text = "second", .active = false, .bankPosition = mrt::plugin::StyleCardBankPosition { .column = 0, .row = 0 } });
    deck.addCard ({ .id = "active", .text = "active", .active = true });

    const auto ordered = deck.bankCardsOrdered();

    REQUIRE (ordered.size() == 3);
    CHECK (ordered[0].id == "b");
    CHECK (ordered[1].id == "a");
    CHECK (ordered[2].id == "z");
}

TEST_CASE ("StyleCardDeck replace uses dissolve-only transition state and clamps replacement weight", "[style-cards]")
{
    mrt::plugin::StyleCardDeck deck;
    deck.setTransitionDurationSeconds (0.01);
    deck.addCard ({ .id = "old", .text = "house", .weight = 0.5f, .active = true });

    REQUIRE (deck.replaceCard ("old", { .id = "new", .text = "jungle", .weight = 7.5f, .active = true }, 12.0));

    const auto& cards = deck.cards();
    REQUIRE (cards.size() == 1);
    CHECK (cards[0].id == "new");
    CHECK (cards[0].weight == Catch::Approx (2.0f));
    CHECK (cards[0].transition.inProgress);
    CHECK (cards[0].transition.fromActive);
    CHECK (cards[0].transition.toActive);
    CHECK (cards[0].transition.startSeconds == Catch::Approx (12.0));
    CHECK (cards[0].transition.durationSeconds == Catch::Approx (0.1));
}

TEST_CASE ("StyleCardDeck updates card text weight and active slot placement", "[style-cards]")
{
    mrt::plugin::StyleCardDeck deck;
    deck.addCard ({ .id = "a", .text = "house", .weight = -1.0f, .active = false, .bankPosition = mrt::plugin::StyleCardBankPosition { .column = 1, .row = 0 } });
    deck.addCard ({ .id = "b", .text = "garage", .active = true });

    REQUIRE (deck.updateText ("a", "ambient"));
    REQUIRE (deck.updateWeight ("a", 5.0f));
    REQUIRE (deck.moveCardToActiveSlot ("a", 0, 6.0));

    const auto& cards = deck.cards();
    REQUIRE (cards.size() == 2);
    CHECK (cards[0].id == "a");
    CHECK (cards[0].text == "ambient");
    CHECK (cards[0].weight == Catch::Approx (2.0f));
    CHECK (cards[0].active);
    CHECK_FALSE (cards[0].bankPosition.has_value());
    CHECK (cards[0].transition.inProgress);
    CHECK_FALSE (cards[0].transition.fromActive);
    CHECK (cards[0].transition.toActive);
}

TEST_CASE ("Default style cards reference bundled generated SVG backgrounds", "[style-cards]")
{
    auto deck = mrt::plugin::makeDefaultStyleCardDeck();
    REQUIRE_FALSE (deck.cards().empty());
    for (const auto& card : deck.cards())
    {
        CHECK_FALSE (card.imageKey.empty());
        CHECK (card.imageKey.find ("style-backgrounds/") == 0);
        CHECK (card.imageKey.rfind (".svg") == card.imageKey.size() - 4);
    }
}

TEST_CASE ("Default style cards include visible solitaire bank cards", "[style-cards]")
{
    auto deck = mrt::plugin::makeDefaultStyleCardDeck();

    const auto bankCards = deck.bankCardsOrdered();

    REQUIRE (bankCards.size() >= 65);
    std::array<int, 11> columnCounts {};
    for (const auto& card : bankCards)
    {
        CHECK (card.active == ! card.text.starts_with ("TODO:"));
        REQUIRE (card.bankPosition.has_value());
        REQUIRE (card.bankPosition->column >= 0);
        REQUIRE (card.bankPosition->column < static_cast<int> (columnCounts.size()));
        ++columnCounts[static_cast<std::size_t> (card.bankPosition->column)];
    }

    for (std::size_t column = 0; column < columnCounts.size() - 1; ++column)
        CHECK (columnCounts[column] <= 7);
    CHECK (columnCounts.back() >= 3);
}

TEST_CASE ("Default style cards use the requested initial live prompts", "[style-cards]")
{
    const auto deck = mrt::plugin::makeDefaultStyleCardDeck();
    const std::array<std::string, 4> expected {
        "dubstep wobble bass",
        "euphoric festival anthem",
        "lush string adagio",
        "salsa brass hits"
    };

    std::vector<std::string> livePrompts;
    for (const auto& card : deck.cards())
        if (! card.bankPosition.has_value())
            livePrompts.push_back (card.text);

    REQUIRE (livePrompts.size() >= expected.size());
    for (std::size_t i = 0; i < expected.size(); ++i)
        CHECK (livePrompts[i] == expected[i]);
}

TEST_CASE ("Default bank cards use seven rows per prompt column and placeholders only in the last column", "[style-cards]")
{
    const auto deck = mrt::plugin::makeDefaultStyleCardDeck();
    const auto bankCards = deck.bankCardsOrdered();
    const std::vector<std::string> expectedPrompts {
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

    REQUIRE (bankCards.size() >= expectedPrompts.size());
    for (std::size_t i = 0; i < expectedPrompts.size(); ++i)
    {
        CAPTURE (i);
        CHECK (bankCards[i].text == expectedPrompts[i]);
        REQUIRE (bankCards[i].bankPosition.has_value());
        CHECK (bankCards[i].bankPosition->column == static_cast<int> (i / 7));
        CHECK (bankCards[i].bankPosition->row == static_cast<int> (i % 7));
    }

    const int placeholderColumn = static_cast<int> ((expectedPrompts.size() + 6) / 7);
    for (std::size_t i = expectedPrompts.size(); i < bankCards.size(); ++i)
    {
        CAPTURE (i);
        CHECK (bankCards[i].text.starts_with ("TODO:"));
        REQUIRE (bankCards[i].bankPosition.has_value());
        CHECK (bankCards[i].bankPosition->column == placeholderColumn);
    }

    const auto bankBounds = juce::Rectangle<int> { 24, 220, 760, 420 };
    const auto layout = mrt::plugin::computeStyleCardBankLayout (deck, bankBounds);
    REQUIRE (layout.cardBounds.size() >= bankCards.size());
    CHECK (layout.cardBounds[7].getX() > layout.cardBounds[0].getX());
    CHECK (layout.cardBounds[7].getY() == layout.cardBounds[0].getY());
    CHECK (layout.cardBounds[bankCards.size() - 1].getRight() > bankBounds.getRight());
}

TEST_CASE ("StyleCardDeck keeps card colours saturated enough for active cards", "[style-cards]")
{
    mrt::plugin::StyleCardDeck deck;
    deck.addCard ({ .id = "grey-user-card", .text = "new style", .colourArgb = 0xff64748b });

    REQUIRE (deck.cards().size() == 1);
    CHECK (colourSaturation (deck.cards()[0].colourArgb) >= 0.45f);

    const auto defaultDeck = mrt::plugin::makeDefaultStyleCardDeck();
    for (const auto& card : defaultDeck.cards())
        CHECK (colourSaturation (card.colourArgb) >= 0.45f);
}

TEST_CASE ("StyleCardDeck replaces an active slot from an existing bank card", "[style-cards]")
{
    mrt::plugin::StyleCardDeck deck;
    deck.addCard ({ .id = "active", .text = "active", .weight = 0.9f, .active = true });
    deck.addCard ({ .id = "bank",
        .text = "bank",
        .weight = 0.5f,
        .active = false,
        .bankPosition = mrt::plugin::StyleCardBankPosition { .column = 1, .row = 2 } });

    REQUIRE (deck.replaceCardFromExisting ("active", "bank", 7.0));

    const auto& cards = deck.cards();
    REQUIRE (cards.size() == 2);
    CHECK (cards[0].id == "bank");
    CHECK (cards[0].active);
    CHECK_FALSE (cards[0].bankPosition.has_value());
    CHECK (cards[0].transition.inProgress);
    CHECK_FALSE (cards[0].transition.fromActive);
    CHECK (cards[0].transition.toActive);

    CHECK (cards[1].id == "active");
    CHECK (cards[1].active);
    REQUIRE (cards[1].bankPosition.has_value());
    CHECK (cards[1].bankPosition->column == 1);
    CHECK (cards[1].bankPosition->row == 2);
}

TEST_CASE ("StyleCardDeck keeps bank cards active but out of the effective mix", "[style-cards]")
{
    mrt::plugin::StyleCardDeck deck;
    deck.setTransitionDurationSeconds (4.0);
    deck.addCard ({ .id = "bank",
        .text = "bank",
        .weight = 0.5f,
        .active = false,
        .bankPosition = mrt::plugin::StyleCardBankPosition { .column = 0, .row = 0 } });
    deck.addCard ({ .id = "active", .text = "active", .weight = 0.8f, .active = true });

    REQUIRE (deck.cards()[0].active);
    auto initial = deck.effectivePromptPortfolio (0.0).activeSlotsOrdered();
    REQUIRE (initial.size() == 1);
    CHECK (initial[0].text == "active");

    REQUIRE (deck.appendCardToBankColumn ("active", 0, 10.0));
    CHECK (deck.cards()[1].active);
    REQUIRE (deck.cards()[1].bankPosition.has_value());

    auto half = deck.effectivePromptPortfolio (12.0).activeSlotsOrdered();
    REQUIRE (half.size() == 1);
    CHECK (half[0].text == "active");
    CHECK (half[0].weight == Catch::Approx (0.4f));

    CHECK (deck.effectivePromptPortfolio (14.0).activeSlotsOrdered().empty());
}

TEST_CASE ("StyleCardDeck keeps TODO cards inactive and out of the effective mix", "[style-cards]")
{
    mrt::plugin::StyleCardDeck deck;
    deck.addCard ({ .id = "todo", .text = "TODO: your favorite instrument", .weight = 0.9f, .active = true });

    REQUIRE (deck.cards().size() == 1);
    CHECK_FALSE (deck.cards()[0].active);
    CHECK (deck.effectivePromptPortfolio (0.0).activeSlotsOrdered().empty());

    REQUIRE (deck.setActive ("todo", true, 2.0));
    CHECK_FALSE (deck.cards()[0].active);
    CHECK (deck.effectivePromptPortfolio (6.0).activeSlotsOrdered().empty());

    REQUIRE (deck.moveCardToActiveSlot ("todo", 0, 8.0));
    CHECK_FALSE (deck.cards()[0].active);
    CHECK (deck.effectivePromptPortfolio (12.0).activeSlotsOrdered().empty());
}

TEST_CASE ("StyleCardDeck allows edited TODO cards to become active prompts", "[style-cards]")
{
    mrt::plugin::StyleCardDeck deck;
    deck.addCard ({ .id = "todo",
        .text = "TODO: an energetic genre",
        .weight = 0.7f,
        .active = true,
        .bankPosition = mrt::plugin::StyleCardBankPosition { .column = 0, .row = 0 } });

    REQUIRE (deck.updateText ("todo", "driving breakbeat pulse"));
    REQUIRE (deck.moveCardToActiveSlot ("todo", 0, 4.0));
    REQUIRE (deck.setActive ("todo", true, 4.0));

    const auto activeSlots = deck.effectivePromptPortfolio (8.0).activeSlotsOrdered();
    REQUIRE (activeSlots.size() == 1);
    CHECK (activeSlots[0].text == "driving breakbeat pulse");
    CHECK (activeSlots[0].weight == Catch::Approx (0.7f));
}

TEST_CASE ("Default style cards keep editable TODO placeholders", "[style-cards]")
{
    const auto deck = mrt::plugin::makeDefaultStyleCardDeck();

    const auto todoCount = std::count_if (deck.cards().begin(), deck.cards().end(), [] (const mrt::plugin::StyleCard& card) {
        return card.text.starts_with ("TODO:");
    });
    CHECK (todoCount >= 3);
}
