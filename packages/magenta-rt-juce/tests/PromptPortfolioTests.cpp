#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "engine/PromptPortfolio.h"

TEST_CASE ("PromptPortfolio filters and clamps active prompts", "[prompt-portfolio]")
{
    mrt::plugin::PromptPortfolio portfolio (4);

    portfolio.setSlot (0, " ambient pads ", 1.5f);
    portfolio.setSlot (1, "", 2.0f);
    portfolio.setSlot (2, "busy drums", 0.0f);
    portfolio.setSlot (3, "deep bass", 3.0f);

    const auto active = portfolio.activeSlots();

    REQUIRE (active.size() == 2);
    CHECK (active[0].text == "ambient pads");
    CHECK (active[0].weight == Catch::Approx (1.5f));
    CHECK (active[1].text == "deep bass");
    CHECK (active[1].weight == Catch::Approx (2.0f));
    CHECK (portfolio.totalActiveWeight() == Catch::Approx (3.5f));
}

TEST_CASE ("PromptPortfolio picks the highest weighted prompt as MLX fallback", "[prompt-portfolio]")
{
    mrt::plugin::PromptPortfolio portfolio (3);

    portfolio.setSlot (0, "lo-fi tape", 0.75f);
    portfolio.setSlot (1, "glitch percussion", 1.25f);
    portfolio.setSlot (2, "warm strings", 1.25f);

    const auto selected = portfolio.primaryPrompt();

    REQUIRE (selected.has_value());
    CHECK (selected->text == "glitch percussion");
    CHECK (selected->weight == Catch::Approx (1.25f));
}

TEST_CASE ("PromptPortfolio clamps negative and oversized weights", "[prompt-portfolio]")
{
    mrt::plugin::PromptPortfolio portfolio (2);
    portfolio.setSlot (0, "a", -1.0f);
    CHECK (portfolio.activeSlots().empty());

    portfolio.setSlot (1, "b", 5.0f);
    const auto active = portfolio.activeSlots();
    REQUIRE (active.size() == 1);
    CHECK (active[0].text == "b");
    CHECK (active[0].weight == Catch::Approx (2.0f));

    portfolio.setSlot (0, "a", 0.5f);
    portfolio.setSlot (1, "b", 2.0f);
    const auto active2 = portfolio.activeSlots();
    REQUIRE (active2.size() == 2);
    CHECK (active2[0].weight == Catch::Approx (0.5f));
    CHECK (active2[1].weight == Catch::Approx (2.0f));
}

TEST_CASE ("PromptPortfolio preserves active slots in UI order", "[prompt-portfolio]")
{
    mrt::plugin::PromptPortfolio portfolio (4);
    portfolio.setSlot (2, "z last slot", 1.0f);
    portfolio.setSlot (0, "first", 2.0f);
    portfolio.setSlot (1, "mid", 0.5f);

    const auto ordered = portfolio.activeSlotsOrdered();
    REQUIRE (ordered.size() == 3);
    CHECK (ordered[0].slotIndex == 0);
    CHECK (ordered[0].text == "first");
    CHECK (ordered[1].slotIndex == 1);
    CHECK (ordered[1].text == "mid");
    CHECK (ordered[2].slotIndex == 2);
    CHECK (ordered[2].text == "z last slot");
}

TEST_CASE ("PromptPortfolio normalizes active prompt weights", "[prompt-portfolio]")
{
    mrt::plugin::PromptPortfolio portfolio (3);
    portfolio.setSlot (0, "a", 0.5f);
    portfolio.setSlot (1, "b", 1.5f);

    const auto norm = portfolio.normalizedActivePrompts();
    REQUIRE (norm.size() == 2);
    CHECK (norm[0].normalizedWeight == Catch::Approx (0.25f));
    CHECK (norm[1].normalizedWeight == Catch::Approx (0.75f));
}

TEST_CASE ("PromptPortfolio returns no normalized prompts when all inactive", "[prompt-portfolio]")
{
    mrt::plugin::PromptPortfolio portfolio (2);
    portfolio.setSlot (0, "", 1.0f);
    portfolio.setSlot (1, "x", 0.0f);

    CHECK (portfolio.normalizedActivePrompts().empty());
    CHECK (portfolio.signature().empty());
}

TEST_CASE ("PromptPortfolio signature changes with text and weight", "[prompt-portfolio]")
{
    mrt::plugin::PromptPortfolio portfolio (2);
    portfolio.setSlot (0, "deep house", 1.0f);
    const auto s1 = portfolio.signature();
    portfolio.setSlot (0, "deep house", 1.5f);
    const auto s2 = portfolio.signature();
    portfolio.setSlot (0, "dub techno", 1.5f);
    const auto s3 = portfolio.signature();

    CHECK (s1 != s2);
    CHECK (s2 != s3);
    CHECK (s1.find ("0:deep house:") == 0);
}
