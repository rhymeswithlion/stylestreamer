#include "helpers/test_helpers.h"
#include "PluginEditor.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <juce_gui_basics/juce_gui_basics.h>

#include <algorithm>
#include <cstdlib>
#include <string_view>

namespace
{

template <typename ComponentType, typename Predicate>
[[nodiscard]] ComponentType* findChildComponentMatching (juce::Component& root, Predicate&& predicate)
{
    for (auto* child : root.getChildren())
    {
        if (auto* typed = dynamic_cast<ComponentType*> (child);
            typed != nullptr && predicate (*typed))
            return typed;

        if (auto* nested = findChildComponentMatching<ComponentType> (
                *child,
                std::forward<Predicate> (predicate)))
            return nested;
    }

    return nullptr;
}

[[nodiscard]] float snapshotScaleFactor()
{
    if (const char* s = std::getenv ("MRT_JUCE_UI_SNAPSHOT_SCALE"))
    {
        const auto v = static_cast<float> (std::atof (s));
        if (v > 0.0f)
            return juce::jlimit (1.0f, 4.0f, v);
    }

    return 2.0f;
}

[[nodiscard]] bool uiSnapshotWriteEnabled()
{
    const char* flag = std::getenv ("MRT_JUCE_UI_SNAPSHOT");
    if (flag == nullptr)
        return false;

    const std::string_view value { flag };
    return value != "0" && value != "false" && value != "FALSE";
}

[[nodiscard]] juce::File resolveSnapshotOutputFile()
{
    if (const char* dir = std::getenv ("MRT_JUCE_UI_SNAPSHOT_DIR"))
    {
        const juce::File base { juce::String { juce::CharPointer_UTF8 { dir } } };
        return base.getChildFile ("plugin-editor.png");
    }

    const juce::String rootPath { juce::CharPointer_UTF8 { MRT_MONOREPO_ROOT_FOR_TESTS } };
    if (rootPath.isEmpty())
        return {};

    const auto now = juce::Time::getCurrentTime();
    const auto date = now.formatted ("%Y-%m-%d");

    return juce::File { rootPath }
        .getChildFile ("output")
        .getChildFile (date + "-juce-editor-snapshot")
        .getChildFile (date + "-plugin-editor.png");
}

} // namespace

TEST_CASE ("PluginEditor renders to an image", "[ui]")
{
    runWithinPluginEditor ([] (PluginProcessor& plugin) {
        auto* editor = plugin.getActiveEditor();
        REQUIRE (editor != nullptr);

        editor->setSize (900, 900);
        editor->setVisible (true);
        editor->toFront (false);
        editor->resized();
        editor->repaint();
        juce::MessageManager::getInstance()->runDispatchLoopUntil (150);

        const float scale = snapshotScaleFactor();
        const auto image =
            editor->createComponentSnapshot (editor->getLocalBounds(), true, scale);

        REQUIRE (! image.isNull());
        CHECK (image.getWidth() == juce::roundToInt ((float) editor->getWidth() * scale));
        CHECK (image.getHeight() == juce::roundToInt ((float) editor->getHeight() * scale));

        if (! uiSnapshotWriteEnabled())
            return;

        const juce::File outFile = resolveSnapshotOutputFile();
        REQUIRE_FALSE (outFile.getFullPathName().isEmpty());

        REQUIRE (outFile.getParentDirectory().createDirectory());
        (void) outFile.deleteFile();

        juce::FileOutputStream stream (outFile);
        REQUIRE (stream.openedOk());

        juce::PNGImageFormat png;
        REQUIRE (png.writeImageToStream (image, stream));
        stream.flush();

        REQUIRE (outFile.existsAsFile());
    });
}

TEST_CASE ("PluginEditor keeps chunk length control in Advanced options", "[ui]")
{
    runWithinPluginEditor ([] (PluginProcessor& plugin) {
        auto* editor = dynamic_cast<PluginEditor*> (plugin.getActiveEditor());
        REQUIRE (editor != nullptr);
        editor->setSize (900, 900);
        editor->setVisible (true);
        editor->resized();

        auto* advancedButton = findChildComponentMatching<juce::TextButton> (
            *editor,
            [] (const juce::TextButton& button) { return button.getButtonText() == "Advanced..."; });
        REQUIRE (advancedButton != nullptr);

        advancedButton->triggerClick();
        juce::MessageManager::getInstance()->runDispatchLoopUntil (50);

        editor->closeAdvancedOptionsWindow();
        juce::MessageManager::getInstance()->runDispatchLoopUntil (50);

        auto* chunkLabel = findChildComponentMatching<juce::Label> (
            *editor,
            [] (const juce::Label& label) { return label.getText() == "Chunk ms"; });
        REQUIRE (chunkLabel != nullptr);
        CHECK_FALSE (chunkLabel->isVisible());
    });
}

TEST_CASE ("PluginEditor does not expose shuffle prompts button", "[ui]")
{
    runWithinPluginEditor ([] (PluginProcessor& plugin) {
        auto* editor = dynamic_cast<PluginEditor*> (plugin.getActiveEditor());
        REQUIRE (editor != nullptr);
        editor->setSize (900, 900);
        editor->setVisible (true);
        editor->resized();

        auto* shuffleButton = findChildComponentMatching<juce::TextButton> (
            *editor,
            [] (const juce::TextButton& button) { return button.getButtonText() == "Shuffle prompts"; });
        CHECK (shuffleButton == nullptr);
    });
}

TEST_CASE ("PluginEditor shows style cards surface on main screen", "[ui][style-cards]")
{
    runWithinPluginEditor ([] (PluginProcessor& plugin) {
        auto* editor = dynamic_cast<PluginEditor*> (plugin.getActiveEditor());
        REQUIRE (editor != nullptr);
        editor->setSize (900, 900);
        editor->setVisible (true);
        editor->resized();

        CHECK (editor->hasStyleCardsSurfaceForTesting());
    });
}

TEST_CASE ("PluginEditor makes style cards the primary dashboard surface", "[ui][style-cards]")
{
    runWithinPluginEditor ([] (PluginProcessor& plugin) {
        auto* editor = dynamic_cast<PluginEditor*> (plugin.getActiveEditor());
        REQUIRE (editor != nullptr);
        editor->setSize (900, 900);
        editor->setVisible (true);
        editor->resized();

        auto* styleCards = findChildComponentMatching<mrt::plugin::StyleCardsPanel> (
            *editor,
            [] (const mrt::plugin::StyleCardsPanel&) { return true; });
        REQUIRE (styleCards != nullptr);
        CHECK (styleCards->isVisible());
        CHECK (styleCards->getBounds().getHeight() >= 500);

        auto* advancedButton = findChildComponentMatching<juce::TextButton> (
            *editor,
            [] (const juce::TextButton& button) { return button.getButtonText() == "Advanced..."; });
        REQUIRE (advancedButton != nullptr);
        CHECK (advancedButton->isVisible());
        CHECK (advancedButton->getBounds().getY() > styleCards->getBounds().getBottom());
    });
}

TEST_CASE ("PluginEditor copies style cards in prompt state payload", "[ui][style-cards]")
{
    runWithinPluginEditor ([] (PluginProcessor& plugin) {
        auto* editor = dynamic_cast<PluginEditor*> (plugin.getActiveEditor());
        REQUIRE (editor != nullptr);

        const auto payload = editor->promptStatePayloadForTesting();

        CHECK (payload.version == 3);
        REQUIRE_FALSE (payload.styleCards.empty());
        CHECK_FALSE (payload.styleCards[0].imageKey.isEmpty());
    });
}

TEST_CASE ("PluginEditor copies and pastes live slot slider weights", "[ui][style-cards]")
{
    runWithinPluginEditor ([] (PluginProcessor& plugin) {
        auto* editor = dynamic_cast<PluginEditor*> (plugin.getActiveEditor());
        REQUIRE (editor != nullptr);
        editor->setSize (900, 900);
        editor->resized();

        auto* styleCards = findChildComponentMatching<mrt::plugin::StyleCardsPanel> (
            *editor,
            [] (const mrt::plugin::StyleCardsPanel&) { return true; });
        REQUIRE (styleCards != nullptr);

        auto* firstSlider = styleCards->activeWeightSliderForTesting (0);
        REQUIRE (firstSlider != nullptr);
        firstSlider->setValue (42.0, juce::sendNotificationSync);

        auto payload = editor->promptStatePayloadForTesting();
        CHECK (payload.prompts[0].weightPercent == Catch::Approx (42.0));
        REQUIRE (payload.styleCards.size() >= 2);
        std::swap (payload.styleCards[0], payload.styleCards[1]);
        payload.prompts[0].weightPercent = 11.0;
        payload.prompts[1].weightPercent = 89.0;

        editor->applyPromptStatePayloadForTesting (payload);

        auto* pastedFirstSlider = styleCards->activeWeightSliderForTesting (0);
        auto* pastedSecondSlider = styleCards->activeWeightSliderForTesting (1);
        REQUIRE (pastedFirstSlider != nullptr);
        REQUIRE (pastedSecondSlider != nullptr);
        CHECK (pastedFirstSlider->getValue() == Catch::Approx (11.0));
        CHECK (pastedSecondSlider->getValue() == Catch::Approx (89.0));
    });
}

TEST_CASE ("PluginEditor starts with visible solitaire bank cards", "[ui][style-cards]")
{
    runWithinPluginEditor ([] (PluginProcessor& plugin) {
        auto* editor = dynamic_cast<PluginEditor*> (plugin.getActiveEditor());
        REQUIRE (editor != nullptr);

        const auto payload = editor->promptStatePayloadForTesting();
        const auto seededBankCard = std::find_if (
            payload.styleCards.begin(),
            payload.styleCards.end(),
            [] (const mrt::plugin::PromptStateClipboardStyleCard& card) {
                return card.id == "bank-prompt-65" && card.active;
            });

        REQUIRE (seededBankCard != payload.styleCards.end());
        CHECK (seededBankCard->bankColumn.has_value());
        CHECK (seededBankCard->bankRow.has_value());
    });
}
