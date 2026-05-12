#include "PluginEditor.h"

#include "engine/HfWeightDownloader.h"
#include "engine/HfWeightsCache.h"
#include "engine/PromptStateClipboard.h"

#include <juce_gui_extra/juce_gui_extra.h>

#include <array>
#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>

namespace
{

constexpr int mainEditorWidth = 900;
constexpr int styleCardsPanelHeight = 686;
constexpr int mainEditorHeight = 820;
constexpr int advancedWindowWidth = 780;
constexpr int advancedWindowHeight = 720;

[[nodiscard]] juce::Colour panelBackgroundColour() noexcept
{
    return juce::Colour (0xffe5e7eb);
}

/** Opacity for control surfaces (sliders, text fields, buttons) over the dashboard. */
[[nodiscard]] constexpr float widgetFillAlpha() noexcept
{
    return 0.9f;
}

[[nodiscard]] juce::String resolveWeightsDirectoryHint()
{
    if (const char* env = std::getenv ("MRT_JUCE_WEIGHTS_DIR"))
    {
        const juce::File fromEnv { juce::String (env) };
        if (fromEnv.isDirectory())
            return fromEnv.getFullPathName();
    }

    // ``make juce-dist`` copies ``.weights-cache`` into each platform layout:
    // macOS bundle: Contents/Resources/model-weights
    // Linux/binary: sibling ``model-weights`` next to the executable.
    const juce::File exeLocation =
        juce::File::getSpecialLocation (juce::File::currentExecutableFile);
    const juce::File macBundledWeights =
        exeLocation.getParentDirectory()
            .getParentDirectory()
            .getChildFile ("Resources/model-weights");
    if (macBundledWeights.isDirectory())
        return macBundledWeights.getFullPathName();

    const juce::File sidecarBundledWeights =
        exeLocation.getParentDirectory().getChildFile ("model-weights");
    if (sidecarBundledWeights.isDirectory())
        return sidecarBundledWeights.getFullPathName();

    mrt::plugin::HfWeightsCacheConfig hfConfig;
    hfConfig.requireMlxfn = true;
    if (const auto snapshot = mrt::plugin::findCompleteHfWeightsSnapshot (hfConfig))
        return juce::File (snapshot->string()).getFullPathName();

    juce::File walk = juce::File::getSpecialLocation (juce::File::currentApplicationFile)
                         .getParentDirectory();
    for (int i = 0; i < 28 && walk.exists(); ++i)
    {
        const juce::File candidate = walk.getChildFile (".weights-cache");
        if (candidate.isDirectory())
            return candidate.getFullPathName();
        walk = walk.getParentDirectory();
    }

    const juce::File cwdCache =
        juce::File::getCurrentWorkingDirectory().getChildFile (".weights-cache");
    if (cwdCache.isDirectory())
        return cwdCache.getFullPathName();

    return juce::File (mrt::plugin::expectedHfWeightsCacheDirectory (hfConfig).string())
        .getChildFile ("snapshots")
        .getFullPathName();
}

[[nodiscard]] juce::String resolveDepthformerWeightsHint()
{
    if (const char* env = std::getenv ("MRT_JUCE_DEPTHFORMER_WEIGHTS"))
    {
        const juce::File fromEnv { juce::String (env) };
        if (fromEnv.exists())
            return fromEnv.getFullPathName();
    }

    return {};
}

[[nodiscard]] mrt::plugin::PromptStateClipboardPayload defaultPromptStatePayload()
{
    mrt::plugin::PromptStateClipboardPayload payload;
    payload.version = 3;

    const auto defaultDeck = mrt::plugin::makeDefaultStyleCardDeck();
    for (std::size_t i = 0; i < payload.prompts.size() && i < defaultDeck.cards().size(); ++i)
    {
        const auto& card = defaultDeck.cards()[i];
        payload.prompts[i] = { juce::String (card.text), static_cast<double> (defaultDeck.activeSlotWeight (static_cast<int> (i)) * 100.0f) };
    }

    payload.styleCards.reserve (defaultDeck.cards().size());
    for (const auto& card : defaultDeck.cards())
    {
        mrt::plugin::PromptStateClipboardStyleCard clipboardCard {
            .id = juce::String (card.id),
            .text = juce::String (card.text),
            .weightPercent = static_cast<double> (card.weight * 100.0f),
            .active = card.active,
            .colourArgb = card.colourArgb,
            .imageKey = juce::String (card.imageKey),
            .userCreated = card.userCreated
        };
        if (card.bankPosition.has_value())
        {
            clipboardCard.bankColumn = card.bankPosition->column;
            clipboardCard.bankRow = card.bankPosition->row;
        }
        payload.styleCards.push_back (std::move (clipboardCard));
    }

    payload.settings.seed = {};
    payload.settings.temperature = 1.19;
    payload.settings.topK = 44;
    payload.settings.guidanceWeight = 5.0;
    payload.settings.prebufferChunks = 2;
    payload.settings.maxQueueChunks = 3;
    return payload;
}

[[nodiscard]] std::vector<mrt::plugin::PromptStateClipboardStyleCard> clipboardCardsFromDeck (
    const mrt::plugin::StyleCardDeck& deck)
{
    std::vector<mrt::plugin::PromptStateClipboardStyleCard> cards;
    cards.reserve (deck.cards().size());
    for (const auto& card : deck.cards())
    {
        cards.push_back ({
            .id = juce::String (card.id),
            .text = juce::String (card.text),
            .weightPercent = static_cast<double> (card.weight * 100.0f),
            .active = card.active,
            .colourArgb = card.colourArgb,
            .imageKey = juce::String (card.imageKey),
            .userCreated = card.userCreated,
            .bankColumn = card.bankPosition.has_value() ? std::optional<int> (card.bankPosition->column) : std::nullopt,
            .bankRow = card.bankPosition.has_value() ? std::optional<int> (card.bankPosition->row) : std::nullopt
        });
    }
    return cards;
}

[[nodiscard]] mrt::plugin::StyleCardDeck styleCardDeckFromClipboard (
    const std::vector<mrt::plugin::PromptStateClipboardStyleCard>& clipboardCards,
    const std::array<mrt::plugin::PromptStateClipboardSlot, 4>& promptSlots)
{
    mrt::plugin::StyleCardDeck deck;
    for (const auto& card : clipboardCards)
    {
        deck.addCard ({
            .id = card.id.toStdString(),
            .text = card.text.toStdString(),
            .weight = static_cast<float> (card.weightPercent / 100.0),
            .active = card.active,
            .colourArgb = card.colourArgb,
            .imageKey = card.imageKey.toStdString(),
            .userCreated = card.userCreated,
            .bankPosition = card.bankColumn.has_value() && card.bankRow.has_value()
                ? std::optional<mrt::plugin::StyleCardBankPosition> (
                    mrt::plugin::StyleCardBankPosition { .column = *card.bankColumn, .row = *card.bankRow })
                : std::nullopt
        });
    }
    for (std::size_t i = 0; i < promptSlots.size(); ++i)
        deck.updateActiveSlotWeight (static_cast<int> (i), static_cast<float> (promptSlots[i].weightPercent / 100.0));
    return deck;
}

[[nodiscard]] juce::File findWeightHelperScript()
{
    if (const char* env = std::getenv ("MRT_JUCE_WEIGHT_HELPER"))
    {
        const juce::File fromEnv { juce::String (env) };
        if (fromEnv.existsAsFile())
            return fromEnv;
    }

    const juce::File exeLocation =
        juce::File::getSpecialLocation (juce::File::currentExecutableFile);
    const juce::File macBundledHelper =
        exeLocation.getParentDirectory()
            .getParentDirectory()
            .getChildFile ("Resources/weight-helper/download_hf_weights.py");
    if (macBundledHelper.existsAsFile())
        return macBundledHelper;

    const juce::File sidecarHelper =
        exeLocation.getParentDirectory().getChildFile ("weight-helper/download_hf_weights.py");
    if (sidecarHelper.existsAsFile())
        return sidecarHelper;

    juce::File walk = juce::File::getSpecialLocation (juce::File::currentApplicationFile)
                         .getParentDirectory();
    for (int i = 0; i < 28 && walk.exists(); ++i)
    {
        const juce::File candidate =
            walk.getChildFile ("packages/magenta-rt-juce/packaging/weight-helper/download_hf_weights.py");
        if (candidate.existsAsFile())
            return candidate;
        walk = walk.getParentDirectory();
    }

    return juce::File::getCurrentWorkingDirectory().getChildFile (
        "packages/magenta-rt-juce/packaging/weight-helper/download_hf_weights.py");
}

[[nodiscard]] juce::String extractSnapshotDir (const juce::String& output)
{
    juce::StringArray lines;
    lines.addLines (output);
    for (const auto& line : lines)
        if (line.trimStart().startsWith ("SNAPSHOT_DIR="))
            return line.fromFirstOccurrenceOf ("SNAPSHOT_DIR=", false, false).trim();
    return {};
}

} // namespace

juce::String formatRtfStatusTextForDisplay (
    const mrt::plugin::GenerationRtfStats& stats,
    bool downtempoMode,
    int chunkLengthMs)
{
    const auto frameText = juce::String::formatted ("chunk size %dms", chunkLengthMs);
    if (stats.completedChunks == 0)
        return "RTF -  " + frameText;

    if (downtempoMode)
    {
        constexpr double downtempoEffectiveMultiplier = 48000.0 / 32000.0;
        return juce::String::formatted (
            "RTF last %.2fx  avg %.2fx  eff %.2fx  chunk %llu  %s",
            stats.lastChunkRtf,
            stats.averageRtf,
            stats.averageRtf * downtempoEffectiveMultiplier,
            static_cast<unsigned long long> (stats.completedChunks),
            frameText.toRawUTF8());
    }

    return juce::String::formatted (
        "RTF last %.2fx  avg %.2fx  chunk %llu  %s",
        stats.lastChunkRtf,
        stats.averageRtf,
        static_cast<unsigned long long> (stats.completedChunks),
        frameText.toRawUTF8());
}

StyleStreamerLookAndFeel::StyleStreamerLookAndFeel()
{
    const float a = widgetFillAlpha();

    using S = juce::Slider;
    setColour (S::backgroundColourId, juce::Colour (0xffd1d5db).withAlpha (a));
    setColour (S::trackColourId, juce::Colour (0xffdb2777).withAlpha (a));
    setColour (S::thumbColourId, juce::Colour (0xff0891b2).withAlpha (a));
    setColour (S::rotarySliderFillColourId, juce::Colour (0xff0891b2).withAlpha (a));
    setColour (S::rotarySliderOutlineColourId, juce::Colour (0xffdb2777).withAlpha (a));
    setColour (S::textBoxTextColourId, juce::Colour (0xff111827));
    setColour (S::textBoxBackgroundColourId, juce::Colours::white.withAlpha (a));
    setColour (S::textBoxOutlineColourId, juce::Colour (0xff9ca3af).withAlpha (a));
    setColour (S::textBoxHighlightColourId, juce::Colour (0xfffbcfe8).withAlpha (a));

    using L = juce::Label;
    setColour (L::textColourId, juce::Colour (0xff1f2937));
    setColour (L::outlineColourId, juce::Colours::transparentBlack);
    setColour (L::backgroundColourId, juce::Colours::transparentBlack);

    using B = juce::TextButton;
    setColour (B::buttonColourId, juce::Colours::white.withAlpha (a));
    setColour (B::buttonOnColourId, juce::Colour (0xffdb2777).withAlpha (a));
    setColour (B::textColourOffId, juce::Colour (0xff1e3a8a));
    setColour (B::textColourOnId, juce::Colours::white);

    using T = juce::TextEditor;
    setColour (T::backgroundColourId, juce::Colours::white.withAlpha (a));
    setColour (T::outlineColourId, juce::Colour (0xffcbd5e1).withAlpha (a));
    setColour (T::textColourId, juce::Colour (0xff111827));
    setColour (T::focusedOutlineColourId, juce::Colour (0xff0891b2));
    setColour (T::highlightColourId, juce::Colour (0xfffbcfe8).withAlpha (a));
    setColour (T::highlightedTextColourId, juce::Colour (0xff111827));

    using Cb = juce::ComboBox;
    setColour (Cb::backgroundColourId, juce::Colours::white.withAlpha (a));
    setColour (Cb::outlineColourId, juce::Colour (0xffcbd5e1).withAlpha (a));
    setColour (Cb::textColourId, juce::Colour (0xff111827));
    setColour (Cb::buttonColourId, juce::Colour (0xfff1f5f9).withAlpha (a));
    setColour (Cb::arrowColourId, juce::Colour (0xffb45309).withAlpha (a));

    using Tb = juce::ToggleButton;
    setColour (Tb::textColourId, juce::Colour (0xff1f2937));
    setColour (Tb::tickColourId, juce::Colour (0xff0891b2).withAlpha (a));
    setColour (Tb::tickDisabledColourId, juce::Colour (0xff9ca3af).withAlpha (a));

    using P = juce::PopupMenu;
    setColour (P::backgroundColourId, juce::Colours::white.withAlpha (a));
    setColour (P::textColourId, juce::Colour (0xff111827));
    setColour (P::highlightedBackgroundColourId, juce::Colour (0xfffce7f3).withAlpha (a));
    setColour (P::highlightedTextColourId, juce::Colour (0xff111827));
}

void StyleStreamerLookAndFeel::drawToggleButton (juce::Graphics& g,
    juce::ToggleButton& button,
    bool shouldDrawButtonAsHighlighted,
    bool shouldDrawButtonAsDown)
{
    juce::ignoreUnused (shouldDrawButtonAsDown);

    const auto bounds = button.getLocalBounds().toFloat();
    const auto switchBounds = bounds.withWidth (52.0f).reduced (2.0f, 6.0f);
    const bool on = button.getToggleState();
    const float alpha = button.isEnabled() ? 1.0f : 0.45f;

    g.setColour ((on ? juce::Colour (0xff0891b2) : juce::Colour (0xffcbd5e1))
                     .withAlpha (shouldDrawButtonAsHighlighted ? 0.95f : 0.82f)
                     .withMultipliedAlpha (alpha));
    g.fillRoundedRectangle (switchBounds, switchBounds.getHeight() * 0.5f);

    g.setColour (juce::Colours::white.withAlpha (alpha));
    const float knob = switchBounds.getHeight() - 6.0f;
    const float knobX = on ? switchBounds.getRight() - knob - 3.0f
                           : switchBounds.getX() + 3.0f;
    g.fillEllipse (knobX, switchBounds.getY() + 3.0f, knob, knob);

    g.setColour (button.findColour (juce::ToggleButton::textColourId).withAlpha (alpha));
    g.setFont (juce::FontOptions (juce::jmin (15.0f, bounds.getHeight() * 0.48f),
        juce::Font::bold));
    g.drawFittedText (button.getButtonText(),
        button.getLocalBounds().withTrimmedLeft (62).withTrimmedRight (2),
        juce::Justification::centredLeft,
        1);
}

juce::Rectangle<int> PluginEditor::layoutRow (juce::Rectangle<int>& area, int height, int gap)
{
    auto row = area.removeFromTop (height);
    area.removeFromTop (gap);
    return row;
}

bool PluginEditor::advancedControlsOnMainEditor() const noexcept
{
    return advancedWidgetsShownOnMainEditor_;
}

void PluginEditor::reparentAdvancedControlsTo (juce::Component& host)
{
    host.addAndMakeVisible (weightsLabel);
    host.addAndMakeVisible (weightsEditor);
    host.addAndMakeVisible (depthformerWeightsLabel);
    host.addAndMakeVisible (depthformerWeightsEditor);
    host.addAndMakeVisible (dtypeLabel);
    host.addAndMakeVisible (dtypeBox);
    host.addAndMakeVisible (mlxfnToggle);
    host.addAndMakeVisible (seedLabel);
    host.addAndMakeVisible (seedEditor);
    host.addAndMakeVisible (temperatureLabel);
    host.addAndMakeVisible (temperatureSlider);
    host.addAndMakeVisible (topKLabel);
    host.addAndMakeVisible (topKSlider);
    host.addAndMakeVisible (guidanceLabel);
    host.addAndMakeVisible (guidanceSlider);
    host.addAndMakeVisible (prebufferLabel);
    host.addAndMakeVisible (prebufferSlider);
    host.addAndMakeVisible (maxQueueLabel);
    host.addAndMakeVisible (maxQueueSlider);
    host.addAndMakeVisible (chunkLengthLabel);
    host.addAndMakeVisible (chunkLengthSlider);
    host.addAndMakeVisible (styleTransitionDelayLabel);
    host.addAndMakeVisible (styleTransitionDelaySlider);
    host.addAndMakeVisible (guidanceHint);
    host.addAndMakeVisible (downloadWeightsButton);
    host.addAndMakeVisible (inspectButton);
    host.addAndMakeVisible (copyStatusButton);
    host.addAndMakeVisible (statusEditor);
}

void PluginEditor::layoutAdvancedControlsIn (juce::Rectangle<int> bounds)
{
    auto area = bounds.reduced (14, 14);

    auto weightsRow = layoutRow (area, 34, 6);
    weightsLabel.setBounds (weightsRow.removeFromLeft (110));
    weightsEditor.setBounds (weightsRow);

    auto depthformerRow = layoutRow (area, 34, 6);
    depthformerWeightsLabel.setBounds (depthformerRow.removeFromLeft (110));
    depthformerWeightsEditor.setBounds (depthformerRow);

    auto dtypeRow = layoutRow (area, 34, 6);
    dtypeLabel.setBounds (dtypeRow.removeFromLeft (76));
    dtypeBox.setBounds (dtypeRow.removeFromLeft (150));
    dtypeRow.removeFromLeft (12);
    mlxfnToggle.setBounds (dtypeRow);

    area.removeFromTop (2);

    auto seedRow = layoutRow (area, 32, 6);
    seedLabel.setBounds (seedRow.removeFromLeft (52));
    seedEditor.setBounds (seedRow);

    {
        auto slidersRow = layoutRow (area, 78, 6);
        const int colW = juce::jmax (1, slidersRow.getWidth() / 3);
        auto b1 = slidersRow.removeFromLeft (colW);
        temperatureLabel.setBounds (b1.removeFromTop (18));
        temperatureSlider.setBounds (b1);

        auto b2 = slidersRow.removeFromLeft (colW);
        topKLabel.setBounds (b2.removeFromTop (18));
        topKSlider.setBounds (b2);

        auto b3 = slidersRow;
        guidanceLabel.setBounds (b3.removeFromTop (18));
        guidanceSlider.setBounds (b3);
    }

    {
        auto slidersRow2 = layoutRow (area, 78, 6);
        const int colW = juce::jmax (1, slidersRow2.getWidth() / 4);
        auto pb = slidersRow2.removeFromLeft (colW);
        prebufferLabel.setBounds (pb.removeFromTop (18));
        prebufferSlider.setBounds (pb);

        auto mq = slidersRow2.removeFromLeft (colW);
        maxQueueLabel.setBounds (mq.removeFromTop (18));
        maxQueueSlider.setBounds (mq);

        auto cl = slidersRow2;
        chunkLengthLabel.setBounds (cl.removeFromTop (18));
        chunkLengthSlider.setBounds (cl.removeFromLeft (colW));

        auto st = cl;
        styleTransitionDelayLabel.setBounds (st.removeFromTop (18));
        styleTransitionDelaySlider.setBounds (st);
    }

    guidanceHint.setBounds (layoutRow (area, 64, 6));

    auto toolsRow = layoutRow (area, 34, 6);
    downloadWeightsButton.setBounds (toolsRow.removeFromLeft (150));
    toolsRow.removeFromLeft (8);
    inspectButton.setBounds (toolsRow.removeFromLeft (118));
    toolsRow.removeFromLeft (8);
    copyStatusButton.setBounds (toolsRow.removeFromLeft (118));

    area.removeFromTop (8);
    statusEditor.setBounds (area);
}

class AdvancedOptionsContent : public juce::Component
{
public:
    explicit AdvancedOptionsContent (PluginEditor& ed) : editor (ed)
    {
        setLookAndFeel (&editor.getLookAndFeel());
        editor.reparentAdvancedControlsTo (*this);
    }

    ~AdvancedOptionsContent() override
    {
        setLookAndFeel (nullptr);
        editor.reparentAdvancedControlsTo (editor);
        editor.hideAdvancedWidgetsAfterReparentToEditor();
    }

    void resized() override
    {
        editor.layoutAdvancedControlsIn (getLocalBounds());
    }

private:
    PluginEditor& editor;
};

class StyleStreamerAdvancedWindow : public juce::DocumentWindow
{
public:
    StyleStreamerAdvancedWindow (PluginEditor& editor, std::unique_ptr<AdvancedOptionsContent> content)
        : DocumentWindow ("StyleStreamer - advanced options",
              panelBackgroundColour(),
              juce::DocumentWindow::minimiseButton | juce::DocumentWindow::closeButton),
          pluginEditor (editor)
    {
        setUsingNativeTitleBar (true);
        setResizable (true, true);
        setResizeLimits (520, 620, 1400, 1000);
        setContentOwned (content.release(), true);

        const auto anchor = editor.getScreenBounds();
        constexpr int w = advancedWindowWidth;
        constexpr int h = advancedWindowHeight;
        setBounds (anchor.getCentreX() - w / 2, anchor.getCentreY() - h / 2, w, h);
        setAlwaysOnTop (false);
        setVisible (true);
    }

    void closeButtonPressed() override
    {
        juce::Component::SafePointer<PluginEditor> ed (&pluginEditor);
        juce::MessageManager::callAsync ([ed] {
            if (ed != nullptr)
                ed->closeAdvancedOptionsWindow();
        });
    }

private:
    PluginEditor& pluginEditor;
};

void PluginEditor::openAdvancedOptionsWindow()
{
    if (advancedOptionsWindow_ != nullptr)
    {
        advancedOptionsWindow_->toFront (true);
        return;
    }

    advancedOptionsWindow_ = std::make_unique<StyleStreamerAdvancedWindow> (
        *this,
        std::make_unique<AdvancedOptionsContent> (*this));
    resized();
}

void PluginEditor::closeAdvancedOptionsWindow()
{
    advancedOptionsWindow_.reset();
    resized();
}

void PluginEditor::hideAdvancedWidgetsAfterReparentToEditor()
{
    advancedWidgetsShownOnMainEditor_ = false;

    const auto hide = [] (juce::Component& c) { c.setVisible (false); };
    hide (weightsLabel);
    hide (weightsEditor);
    hide (depthformerWeightsLabel);
    hide (depthformerWeightsEditor);
    hide (dtypeLabel);
    hide (dtypeBox);
    hide (mlxfnToggle);
    hide (seedLabel);
    hide (seedEditor);
    hide (temperatureLabel);
    hide (temperatureSlider);
    hide (topKLabel);
    hide (topKSlider);
    hide (guidanceLabel);
    hide (guidanceSlider);
    hide (prebufferLabel);
    hide (prebufferSlider);
    hide (maxQueueLabel);
    hide (maxQueueSlider);
    hide (chunkLengthLabel);
    hide (chunkLengthSlider);
    hide (styleTransitionDelayLabel);
    hide (styleTransitionDelaySlider);
    hide (guidanceHint);
    hide (downloadWeightsButton);
    hide (inspectButton);
    hide (copyStatusButton);
    hide (statusEditor);
}

PluginEditor::~PluginEditor()
{
    stopTimer();
    advancedOptionsWindow_.reset();
    setLookAndFeel (nullptr);
}

void PluginEditor::loadDistillPromptPool()
{
    distillPromptPool.clear();

    if (BinaryData::musiccoca_textaudio100_prompts_jsonSize <= 0)
        return;

    const juce::String text (BinaryData::musiccoca_textaudio100_prompts_json,
        static_cast<size_t> (BinaryData::musiccoca_textaudio100_prompts_jsonSize));
    const juce::var root = juce::JSON::parse (text);
    auto* obj = root.getDynamicObject();
    if (obj == nullptr || ! obj->hasProperty ("prompts"))
        return;

    const juce::var promptsVar (obj->getProperty ("prompts"));
    if (! promptsVar.isArray())
        return;

    for (const auto& entry : *promptsVar.getArray())
        if (entry.isString())
            distillPromptPool.add (entry.toString());
}

PluginEditor::PluginEditor (PluginProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    setLookAndFeel (&styleLaf_);
    loadDistillPromptPool();
    configureControls();
    applyPromptStatePayloadToUi (defaultPromptStatePayload());

    juce::MemoryInputStream heroStream (BinaryData::stylestreamer_png, BinaryData::stylestreamer_pngSize, false);
    heroImage = juce::ImageFileFormat::loadFrom (heroStream);

    setSize (mainEditorWidth, mainEditorHeight);
    hideAdvancedWidgetsAfterReparentToEditor();
    resized();
    startTimerHz (4);
}

void PluginEditor::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds();

    g.fillAll (panelBackgroundColour());
    g.setColour (juce::Colour (0xffcbd5e1));
    g.drawRect (bounds.toFloat(), 1.0f);
}

void PluginEditor::resized()
{
    auto outer = getLocalBounds();

    constexpr int heroBandH = 0;
    outer.removeFromTop (heroBandH);

    auto area = outer.reduced (18);

    if (advancedControlsOnMainEditor())
    {
        auto dtypeRow = layoutRow (area, 34);
        dtypeLabel.setBounds (dtypeRow.removeFromLeft (90));
        dtypeBox.setBounds (dtypeRow.removeFromLeft (160));
        dtypeRow.removeFromLeft (16);
        mlxfnToggle.setBounds (dtypeRow.removeFromLeft (220));
    }

    auto cardsArea = area.removeFromTop (styleCardsPanelHeight);
    styleCardsPanel_.setBounds (cardsArea);
    auto rtfHeaderArea = cardsArea.reduced (18).removeFromTop (28);
    rtfStatusLabel.setBounds (rtfHeaderArea.removeFromRight (520));
    rtfStatusLabel.toFront (false);
    area.removeFromTop (10);

    if (advancedControlsOnMainEditor())
    {
        auto controlsRow = layoutRow (area, 78);
        auto controlWidth = controlsRow.getWidth() / 6;
        seedLabel.setBounds (controlsRow.removeFromLeft (controlWidth).removeFromTop (20));
        seedEditor.setBounds (seedLabel.getBounds().translated (0, 24).withHeight (28));
        temperatureLabel.setBounds (controlsRow.removeFromLeft (controlWidth).removeFromTop (20));
        temperatureSlider.setBounds (temperatureLabel.getBounds().translated (0, 24).withHeight (34));
        topKLabel.setBounds (controlsRow.removeFromLeft (controlWidth).removeFromTop (20));
        topKSlider.setBounds (topKLabel.getBounds().translated (0, 24).withHeight (34));
        guidanceLabel.setBounds (controlsRow.removeFromLeft (controlWidth).removeFromTop (20));
        guidanceSlider.setBounds (guidanceLabel.getBounds().translated (0, 24).withHeight (34));
        prebufferLabel.setBounds (controlsRow.removeFromLeft (controlWidth).removeFromTop (20));
        prebufferSlider.setBounds (prebufferLabel.getBounds().translated (0, 24).withHeight (34));
        maxQueueLabel.setBounds (controlsRow.removeFromLeft (controlWidth).removeFromTop (20));
        maxQueueSlider.setBounds (maxQueueLabel.getBounds().translated (0, 24).withHeight (34));

        auto hintRow = layoutRow (area, 52);
        guidanceHint.setBounds (hintRow);
    }

    auto buttonRow = layoutRow (area, 42);
    startButton.setBounds (buttonRow.removeFromLeft (110));
    buttonRow.removeFromLeft (8);
    stopButton.setBounds (buttonRow.removeFromLeft (110));
    buttonRow.removeFromLeft (8);
    resetButton.setBounds (buttonRow.removeFromLeft (110));

    if (advancedControlsOnMainEditor())
    {
        buttonRow.removeFromLeft (8);
        inspectButton.setBounds (buttonRow.removeFromLeft (100));
        buttonRow.removeFromLeft (8);
        copyStatusButton.setBounds (buttonRow.removeFromLeft (96));
    }

    auto mixRow = area.removeFromTop (38);
    copyStateButton.setBounds (mixRow.removeFromLeft (112));
    mixRow.removeFromLeft (10);
    pasteStateButton.setBounds (mixRow.removeFromLeft (112));
    mixRow.removeFromLeft (10);
    downtempoToggle.setBounds (mixRow.removeFromLeft (145));
    mixRow.removeFromLeft (10);
    advancedOptionsButton.setBounds (mixRow.removeFromLeft (170));
}

void PluginEditor::configureControls()
{
    weightsLabel.setText ("Weights", juce::dontSendNotification);
    weightsEditor.setText (resolveWeightsDirectoryHint());
    weightsEditor.setTooltip ("C++ MLX bundle directory (.safetensors). Changing this takes effect when you press Start.");
    weightsLabel.setTooltip (weightsEditor.getTooltip());
    addAndMakeVisible (weightsLabel);
    addAndMakeVisible (weightsEditor);

    depthformerWeightsLabel.setText ("Depthformer", juce::dontSendNotification);
    depthformerWeightsEditor.setText (resolveDepthformerWeightsHint());
    depthformerWeightsEditor.setTooltip (
        "Optional fine-tuned Depthformer .safetensors file or override directory. "
        "SpectroStream, MusicCoCa, vocab, and MLXFN still come from Weights.");
    depthformerWeightsLabel.setTooltip (depthformerWeightsEditor.getTooltip());
    addAndMakeVisible (depthformerWeightsLabel);
    addAndMakeVisible (depthformerWeightsEditor);

    dtypeLabel.setText ("DType", juce::dontSendNotification);
    dtypeBox.addItem ("bf16", 1);
    dtypeBox.addItem ("fp32", 2);
    dtypeBox.addItem ("fp16", 3);
    dtypeBox.setSelectedId (1, juce::dontSendNotification);
    dtypeBox.setTooltip ("Depthformer compute dtype. Changing this takes effect when you press Start.");
    dtypeLabel.setTooltip (dtypeBox.getTooltip());
    mlxfnToggle.setToggleState (true, juce::dontSendNotification);
    mlxfnToggle.setColour (juce::ToggleButton::textColourId, juce::Colour (0xff1f2937));
    mlxfnToggle.setTooltip ("MLXFN fast-path graphs when present. Changing this takes effect when you press Start.");
    addAndMakeVisible (dtypeLabel);
    addAndMakeVisible (dtypeBox);
    addAndMakeVisible (mlxfnToggle);

    const bool havePool = distillPromptPool.size() >= promptSlotCount;
    for (int i = 0; i < promptSlotCount; ++i)
    {
        auto& label = promptLabels[static_cast<std::size_t> (i)];
        auto& editor = promptEditors[static_cast<std::size_t> (i)];
        auto& slider = promptWeights[static_cast<std::size_t> (i)];

        label.setText ("Prompt " + juce::String (i + 1), juce::dontSendNotification);
        editor.setText (havePool ? juce::String() : (i == 0 ? "deep house" : juce::String()), juce::dontSendNotification);
        editor.setTooltip ("Prompt text. Live while running.");
        slider.setRange (0.0, 100.0, 1.0);
        slider.setNumDecimalPlacesToDisplay (0);
        slider.setTextValueSuffix ("%");
        slider.setValue (havePool ? 0.0 : (i == 0 ? 100.0 : 0.0), juce::dontSendNotification);
        slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 64, 22);
        slider.setTooltip ("Relative blend weight for this slot (percent). "
                           "Weights are normalized across non-empty prompts when mixing. Live while running.");
        label.setTooltip ("Prompt slot. Live while running.");

        addAndMakeVisible (label);
        addAndMakeVisible (editor);
        addAndMakeVisible (slider);
        label.setVisible (false);
        editor.setVisible (false);
        slider.setVisible (false);
    }

    styleCardDeck_ = mrt::plugin::makeDefaultStyleCardDeck();
    styleCardDeck_.setTransitionDurationSeconds (4.0);
    styleCardsPanel_.setEventHandler ([this] (const mrt::plugin::StyleCardEvent& event) {
        handleStyleCardEvent (event);
    });
    addAndMakeVisible (styleCardsPanel_);

    seedLabel.setText ("Seed", juce::dontSendNotification);
    seedEditor.setText ("0");
    addAndMakeVisible (seedLabel);
    addAndMakeVisible (seedEditor);

    auto configureSlider = [this] (juce::Label& label, juce::Slider& slider, const juce::String& name,
                               double min, double max, double step, double value) {
        label.setText (name, juce::dontSendNotification);
        slider.setRange (min, max, step);
        slider.setValue (value, juce::dontSendNotification);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 64, 22);
        addAndMakeVisible (label);
        addAndMakeVisible (slider);
    };

    configureSlider (temperatureLabel, temperatureSlider, "Temperature", 0.0, 4.0, 0.01, 1.1);
    configureSlider (topKLabel, topKSlider, "Top K", 0.0, 1024.0, 1.0, 40.0);
    configureSlider (guidanceLabel, guidanceSlider, "CFG weight", 0.0, 10.0, 0.01, 5.0);
    configureSlider (prebufferLabel, prebufferSlider, "Prebuffer", 0.0, 8.0, 1.0, 2.0);
    configureSlider (maxQueueLabel, maxQueueSlider, "Max queue", 1.0, 24.0, 1.0, 3.0);
    configureSlider (chunkLengthLabel, chunkLengthSlider, "Chunk ms", 40.0, 2000.0, 40.0, 400.0);
    configureSlider (styleTransitionDelayLabel, styleTransitionDelaySlider, "Style transition", 0.1, 30.0, 0.1, 4.0);
    styleTransitionDelaySlider.setTextValueSuffix (" s");

    guidanceHint.setText (
        "Classifier-free guidance (Magenta RT guidance_weight): each sampling step sets logits to\n"
        "(1 + w) * conditioned - w * unconditioned (batched conditioned / unconditioned pairs), then samples.\n"
        "Larger w pushes harder toward the prompt; very large values can sound harsh. Gin defaults are often around 4 (merged base).",
        juce::dontSendNotification);
    guidanceHint.setFont (juce::FontOptions (12.0f));
    guidanceHint.setColour (juce::Label::textColourId, juce::Colour (0xff6b7280));
    guidanceHint.setJustificationType (juce::Justification::topLeft);
    addAndMakeVisible (guidanceHint);

    seedEditor.setTooltip ("Per-chunk RNG offset. Live while running.");
    seedLabel.setTooltip (seedEditor.getTooltip());
    temperatureSlider.setTooltip ("Sampling temperature. Live while running.");
    temperatureLabel.setTooltip (temperatureSlider.getTooltip());
    topKSlider.setTooltip ("Top-k sampling cutoff. Live while running.");
    topKLabel.setTooltip (topKSlider.getTooltip());
    guidanceSlider.setTooltip (
        "Classifier-free guidance strength w: guided_logits = (1+w)*conditioned - w*unconditioned "
        "(see magenta_rt.depthformer.decode.decode_with_classifier_free_guidance). Live while running.");
    guidanceLabel.setTooltip (guidanceSlider.getTooltip());
    prebufferSlider.setTooltip ("Chunks to queue before audio plays. Live while running.");
    prebufferLabel.setTooltip (prebufferSlider.getTooltip());
    maxQueueSlider.setTooltip ("Back-pressure limit in chunks. Live while running.");
    maxQueueLabel.setTooltip (maxQueueSlider.getTooltip());
    chunkLengthSlider.setTooltip ("Generated model chunk length in milliseconds. Applies the next time generation starts.");
    chunkLengthLabel.setTooltip (chunkLengthSlider.getTooltip());
    styleTransitionDelaySlider.setTooltip ("Dissolve duration for native style-card state changes. Live while running.");
    styleTransitionDelayLabel.setTooltip (styleTransitionDelaySlider.getTooltip());

    addAndMakeVisible (startButton);
    addAndMakeVisible (stopButton);
    addAndMakeVisible (resetButton);
    addAndMakeVisible (downloadWeightsButton);
    addAndMakeVisible (inspectButton);
    addAndMakeVisible (copyStatusButton);
    addAndMakeVisible (copyStateButton);
    addAndMakeVisible (pasteStateButton);
    addAndMakeVisible (downtempoToggle);
    addAndMakeVisible (advancedOptionsButton);
    startButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xffffd54f).withAlpha (widgetFillAlpha()));
    startButton.setColour (juce::TextButton::textColourOffId, juce::Colour (0xff111827));
    stopButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xffdb2777).withAlpha (widgetFillAlpha()));
    stopButton.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    resetButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xfff1f5f9).withAlpha (widgetFillAlpha()));
    resetButton.setColour (juce::TextButton::textColourOffId, juce::Colour (0xff0891b2));
    downloadWeightsButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xffecfeff).withAlpha (widgetFillAlpha()));
    downloadWeightsButton.setColour (juce::TextButton::textColourOffId, juce::Colour (0xff155e75));
    inspectButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xfff1f5f9).withAlpha (widgetFillAlpha()));
    inspectButton.setColour (juce::TextButton::textColourOffId, juce::Colour (0xffb45309));
    copyStatusButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xfff1f5f9).withAlpha (widgetFillAlpha()));
    copyStatusButton.setColour (juce::TextButton::textColourOffId, juce::Colour (0xff0891b2));
    copyStateButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xffecfeff).withAlpha (widgetFillAlpha()));
    copyStateButton.setColour (juce::TextButton::textColourOffId, juce::Colour (0xff155e75));
    pasteStateButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xfffdf4ff).withAlpha (widgetFillAlpha()));
    pasteStateButton.setColour (juce::TextButton::textColourOffId, juce::Colour (0xff86198f));
    downtempoToggle.setToggleState (processorRef.downtempoMode(), juce::dontSendNotification);
    downtempoToggle.setColour (juce::ToggleButton::textColourId, juce::Colour (0xff1f2937));
    advancedOptionsButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xfff1f5f9).withAlpha (widgetFillAlpha()));
    advancedOptionsButton.setColour (juce::TextButton::textColourOffId, juce::Colour (0xff1e3a8a));
    startButton.setTooltip (
        "Applies weights (Advanced... window) and live fields, stops any previous run, then starts generation.");
    stopButton.setTooltip ("Stop the background generator.");
    resetButton.setTooltip ("Clear the audio queue and reset model state.");
    copyStatusButton.setTooltip ("Copy the full status log to the clipboard.");
    copyStateButton.setTooltip ("Copy prompts and live generation settings as a base64 state string.");
    pasteStateButton.setTooltip ("Paste a base64 state string to restore prompts and live generation settings.");
    downtempoToggle.setTooltip (
        "Play generated 48 kHz audio as if it were 32 kHz: slower, lower pitch, host output stays at 48 kHz.");
    downloadWeightsButton.setTooltip (
        "Download C++ MLX weights into the standard Hugging Face cache, then use the resolved snapshot path.");
    advancedOptionsButton.setTooltip (
        "Open a window for weights paths, Download weights, DType, MLXFN, seed, temperature, top-k, "
        "CFG weight, prebuffer, max queue, chunk length, Inspect UI, and Copy log.");

    rtfStatusLabel.setText (
        formatRtfStatusTextForDisplay (
            {},
            processorRef.downtempoMode(),
            static_cast<int> (std::lround (chunkLengthSlider.getValue()))),
        juce::dontSendNotification);
    rtfStatusLabel.setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 13.0f, juce::Font::plain));
    rtfStatusLabel.setColour (juce::Label::textColourId, juce::Colour (0xff0f766e));
    rtfStatusLabel.setJustificationType (juce::Justification::centredRight);
    rtfStatusLabel.setTooltip (
        "Real-Time Factor = decoded audio duration / wall time for each generate_chunk call "
        "(same definition as tests/test_performance.py PerfResult.rtf). Downtempo mode plays each "
        "generated chunk 1.5x longer, so the label also shows effective playback margin when enabled. "
        "Above 1.0x is faster than real time.");
    addAndMakeVisible (rtfStatusLabel);

    statusEditor.setMultiLine (true, true);
    statusEditor.setReturnKeyStartsNewLine (false);
    statusEditor.setReadOnly (true);
    statusEditor.setCaretVisible (false);
    statusEditor.setScrollbarsShown (true);
    statusEditor.setPopupMenuEnabled (true);
    statusEditor.setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 12.0f, juce::Font::plain));
    statusEditor.setColour (juce::TextEditor::backgroundColourId, juce::Colours::white.withAlpha (widgetFillAlpha()));
    statusEditor.setColour (juce::TextEditor::outlineColourId, juce::Colour (0xffcbd5e1).withAlpha (widgetFillAlpha()));
    statusEditor.setColour (juce::TextEditor::textColourId, juce::Colour (0xff111827));
    addAndMakeVisible (statusEditor);
    setStatusMessage ("Stopped", true);

    startButton.onClick = [this] {
        const juce::File weights (weightsEditor.getText().trim());
        if (! weights.isDirectory())
        {
            setStatusMessage (
                "Weights folder not found. Use Download weights in Advanced..., set MRT_JUCE_WEIGHTS_DIR, "
                "or enter an absolute C++ MLX weights path. Tried: "
                    + weights.getFullPathName(),
                true);
            return;
        }

        const auto depthformerText = depthformerWeightsEditor.getText().trim();
        if (depthformerText.isNotEmpty())
        {
            const juce::File depthformerWeights (depthformerText);
            if (! depthformerWeights.exists())
            {
                setStatusMessage (
                    "Fine-tuned Depthformer path not found. Leave it empty to use the base model, "
                    "or enter a .safetensors file / override directory. Tried: "
                        + depthformerWeights.getFullPathName(),
                    true);
                return;
            }
        }

        applyControlsToProcessor();
        processorRef.startGeneration();
        setStatusMessage ("Starting generation...", true);
    };
    stopButton.onClick = [this] {
        processorRef.stopGeneration();
        setStatusMessage ("Stopped", true);
    };
    resetButton.onClick = [this] {
        processorRef.resetGeneration();
        setStatusMessage ("Reset", true);
    };
    downloadWeightsButton.onClick = [this] { startWeightDownload(); };
    inspectButton.onClick = [&] {
        if (! inspector)
        {
            inspector = std::make_unique<melatonin::Inspector> (*this);
            inspector->onClose = [this]() { inspector.reset(); };
        }

        inspector->setVisible (true);
    };
    copyStatusButton.onClick = [this] {
        juce::SystemClipboard::copyTextToClipboard (statusEditor.getText());
    };
    copyStateButton.onClick = [this] {
        const auto encoded = mrt::plugin::encodePromptStateClipboardPayload (makePromptStatePayloadFromUi());
        juce::SystemClipboard::copyTextToClipboard (encoded);
        setStatusMessage ("Copied prompt state to clipboard:\n" + encoded, true);
    };
    pasteStateButton.onClick = [this] {
        const auto decoded = mrt::plugin::decodePromptStateClipboardPayload (
            juce::SystemClipboard::getTextFromClipboard());
        if (! decoded.ok)
        {
            setStatusMessage ("Could not paste prompt state: " + decoded.error, true);
            return;
        }

        applyPromptStatePayloadToUi (decoded.payload);
        setStatusMessage ("Pasted prompt state from clipboard.", true);
    };
    downtempoToggle.onClick = [this] {
        processorRef.setDowntempoMode (downtempoToggle.getToggleState());
    };
    advancedOptionsButton.onClick = [this] { openAdvancedOptionsWindow(); };

    temperatureSlider.onValueChange = [this] { applyLiveControlsFromUi(); };
    topKSlider.onValueChange = [this] { applyLiveControlsFromUi(); };
    guidanceSlider.onValueChange = [this] { applyLiveControlsFromUi(); };
    prebufferSlider.onValueChange = [this] { applyLiveControlsFromUi(); };
    maxQueueSlider.onValueChange = [this] { applyLiveControlsFromUi(); };
    styleTransitionDelaySlider.onValueChange = [this] { applyLiveControlsFromUi(); };
    seedEditor.onTextChange = [this] { applyLiveControlsFromUi(); };

    for (int i = 0; i < promptSlotCount; ++i)
    {
        promptEditors[static_cast<std::size_t> (i)].onTextChange =
            [this] { applyPromptPortfolioFromUi(); };
        promptWeights[static_cast<std::size_t> (i)].onValueChange =
            [this] { applyPromptPortfolioFromUi(); };
    }
}

void PluginEditor::applyLiveControlsFromUi()
{
    auto live = processorRef.engine().liveControls();
    live.temperature = static_cast<float> (temperatureSlider.getValue());
    live.topK = static_cast<int> (topKSlider.getValue());
    live.guidanceWeight = static_cast<float> (guidanceSlider.getValue());
    live.prebufferChunks = static_cast<int> (prebufferSlider.getValue());
    live.maxQueueChunks = static_cast<int> (maxQueueSlider.getValue());
    live.styleTransitionDelaySeconds = styleTransitionDelaySlider.getValue();
    styleCardDeck_.setTransitionDurationSeconds (styleTransitionDelaySlider.getValue());

    if (seedEditor.getText().isNotEmpty())
        live.seed = static_cast<std::uint64_t> (seedEditor.getText().getLargeIntValue());
    else
        live.seed = std::nullopt;

    processorRef.engine().setLiveControls (std::move (live));
}

void PluginEditor::applyPromptPortfolioFromUi()
{
    mrt::plugin::PromptPortfolio portfolio (promptSlotCount);
    for (int i = 0; i < promptSlotCount; ++i)
    {
        portfolio.setSlot (static_cast<std::size_t> (i),
            promptEditors[static_cast<std::size_t> (i)].getText().toStdString(),
            static_cast<float> (promptWeights[static_cast<std::size_t> (i)].getValue()) / 100.0f);
    }

    processorRef.engine().setPromptPortfolio (std::move (portfolio));
}

void PluginEditor::handleStyleCardEvent (const mrt::plugin::StyleCardEvent& event)
{
    const double now = juce::Time::getMillisecondCounterHiRes() / 1000.0;
    const auto cardId = event.cardId.toStdString();

    if (event.type == "toggle")
    {
        const auto& cards = styleCardDeck_.cards();
        const auto card = std::find_if (cards.begin(), cards.end(), [&cardId] (const mrt::plugin::StyleCard& candidate) {
            return candidate.id == cardId;
        });
        if (card != cards.end())
            styleCardDeck_.setActive (cardId, ! card->active, now);
    }
    else if (event.type == "text")
    {
        styleCardDeck_.updateText (cardId, event.text.toStdString());
    }
    else if (event.type == "weight")
    {
        styleCardDeck_.updateActiveSlotWeight (event.targetSlot, static_cast<float> (event.weightPercent / 100.0));
    }
    else if (event.type == "bankAppend")
    {
        styleCardDeck_.appendCardToBankColumn (cardId, event.targetColumn, now);
    }
    else if (event.type == "replace")
    {
        styleCardDeck_.switchBankCardPosition (cardId, event.targetId.toStdString(), now);
    }
    else if (event.type == "activeSlot")
    {
        styleCardDeck_.moveCardToActiveSlot (cardId, event.targetSlot, now);
    }
    else if (event.type == "bankSwitch")
    {
        styleCardDeck_.switchBankCardPosition (cardId, event.targetId.toStdString(), now);
    }
    else if (event.type == "create")
    {
        const int column = event.targetColumn >= 0 ? event.targetColumn : 4;
        int row = 0;
        for (const auto& card : styleCardDeck_.cards())
        {
            if (card.bankPosition.has_value() && card.bankPosition->column == column)
                row = std::max (row, card.bankPosition->row + 1);
        }

        styleCardDeck_.addCard ({ .id = "user-card-" + std::to_string (styleCardDeck_.cards().size() + 1),
            .text = "new style",
            .weight = 0.5f,
            .active = false,
            .colourArgb = 0xff2563eb,
            .imageKey = "style-backgrounds/ambient-pad.svg",
            .userCreated = true,
            .bankPosition = mrt::plugin::StyleCardBankPosition { .column = column, .row = row } });
    }

    publishEffectiveStyleCards();
}

void PluginEditor::publishEffectiveStyleCards()
{
    const double now = juce::Time::getMillisecondCounterHiRes() / 1000.0;
    lastStyleCardsPublishSeconds_ = now;
    processorRef.engine().setPromptPortfolio (styleCardDeck_.effectivePromptPortfolio (now));
    styleCardsPanel_.setDeck (styleCardDeck_, now);
}

mrt::plugin::PromptStateClipboardPayload PluginEditor::makePromptStatePayloadFromUi() const
{
    mrt::plugin::PromptStateClipboardPayload payload;
    payload.version = 3;
    for (int i = 0; i < promptSlotCount; ++i)
    {
        const auto slot = static_cast<std::size_t> (i);
        const auto& cards = styleCardDeck_.cards();
        const bool hasLiveCard = slot < cards.size() && ! cards[slot].bankPosition.has_value();
        payload.prompts[static_cast<std::size_t> (i)].text =
            hasLiveCard ? juce::String (cards[slot].text) : juce::String();
        payload.prompts[static_cast<std::size_t> (i)].weightPercent =
            static_cast<double> (styleCardDeck_.activeSlotWeight (i) * 100.0f);
    }

    payload.settings.seed = seedEditor.getText();
    payload.settings.temperature = temperatureSlider.getValue();
    payload.settings.topK = static_cast<int> (topKSlider.getValue());
    payload.settings.guidanceWeight = guidanceSlider.getValue();
    payload.settings.prebufferChunks = static_cast<int> (prebufferSlider.getValue());
    payload.settings.maxQueueChunks = static_cast<int> (maxQueueSlider.getValue());
    payload.settings.transitionDelaySeconds = styleTransitionDelaySlider.getValue();
    payload.styleCards = clipboardCardsFromDeck (styleCardDeck_);
    return payload;
}

void PluginEditor::applyPromptStatePayloadToUi (
    const mrt::plugin::PromptStateClipboardPayload& payload)
{
    for (int i = 0; i < promptSlotCount; ++i)
    {
        promptEditors[static_cast<std::size_t> (i)].setText (
            payload.prompts[static_cast<std::size_t> (i)].text,
            juce::dontSendNotification);
        promptWeights[static_cast<std::size_t> (i)].setValue (
            payload.prompts[static_cast<std::size_t> (i)].weightPercent,
            juce::dontSendNotification);
    }

    seedEditor.setText (payload.settings.seed, juce::dontSendNotification);
    temperatureSlider.setValue (payload.settings.temperature, juce::dontSendNotification);
    topKSlider.setValue (payload.settings.topK, juce::dontSendNotification);
    guidanceSlider.setValue (payload.settings.guidanceWeight, juce::dontSendNotification);
    prebufferSlider.setValue (payload.settings.prebufferChunks, juce::dontSendNotification);
    maxQueueSlider.setValue (payload.settings.maxQueueChunks, juce::dontSendNotification);
    styleTransitionDelaySlider.setValue (payload.settings.transitionDelaySeconds, juce::dontSendNotification);

    styleCardDeck_ = styleCardDeckFromClipboard (payload.styleCards, payload.prompts);
    styleCardDeck_.setTransitionDurationSeconds (payload.settings.transitionDelaySeconds);

    applyPromptPortfolioFromUi();
    applyLiveControlsFromUi();
    publishEffectiveStyleCards();
}

void PluginEditor::applyControlsToProcessor()
{
    mrt::plugin::EngineSettings settings;
    settings.weightsDirectory = weightsEditor.getText().trim().toStdString();
    settings.depthformerWeightsPath = depthformerWeightsEditor.getText().trim().toStdString();
    settings.temperature = static_cast<float> (temperatureSlider.getValue());
    settings.topK = static_cast<int> (topKSlider.getValue());
    settings.guidanceWeight = static_cast<float> (guidanceSlider.getValue());
    settings.prebufferChunks = static_cast<int> (prebufferSlider.getValue());
    settings.maxQueueChunks = static_cast<int> (maxQueueSlider.getValue());
    settings.chunkLengthSeconds = static_cast<float> (chunkLengthSlider.getValue() / 1000.0);
    settings.styleTransitionDelaySeconds = styleTransitionDelaySlider.getValue();
    settings.useMlxfn = mlxfnToggle.getToggleState();
    if (seedEditor.getText().isNotEmpty())
        settings.seed = static_cast<std::uint64_t> (seedEditor.getText().getLargeIntValue());

    if (dtypeBox.getSelectedId() == 2)
        settings.dtype = mrt::plugin::ComputeDtype::Float32;
    else if (dtypeBox.getSelectedId() == 3)
        settings.dtype = mrt::plugin::ComputeDtype::Float16;

    processorRef.engine().setPromptPortfolio (
        styleCardDeck_.effectivePromptPortfolio (juce::Time::getMillisecondCounterHiRes() / 1000.0));
    processorRef.engine().configure (std::move (settings));
}

void PluginEditor::setStatusMessage (const juce::String& message, bool scrollToTop)
{
    statusLog_.setMessage (message.toStdString());
    statusEditor.setText (juce::String (statusLog_.visibleText()), juce::dontSendNotification);
    if (scrollToTop)
        statusEditor.moveCaretToTop (false);
}

juce::String PluginEditor::loadedWeightsStatusBlock() const
{
    const juce::String loadedPaths (processorRef.engine().loadedWeightPathsStatus());
    if (loadedPaths.isEmpty())
        return {};

    return "Loaded weights:\n" + loadedPaths;
}

void PluginEditor::setRunningStatusMessage (mrt::plugin::RunningStatus status)
{
    if (statusLog_.updateRunningStatus (
            status,
            loadedWeightsStatusBlock().toStdString()))
    {
        statusEditor.setText (juce::String (statusLog_.visibleText()), juce::dontSendNotification);
    }
}

void PluginEditor::startWeightDownload()
{
    if (weightDownloadRunning_)
    {
        setStatusMessage ("Weight download already running...", false);
        return;
    }

    const auto helperScript = findWeightHelperScript();
    if (! helperScript.existsAsFile())
    {
        setStatusMessage (
            "Weight helper not found. Expected " + helperScript.getFullPathName()
                + "\nSet MRT_JUCE_WEIGHT_HELPER=/path/to/download_hf_weights.py to override.",
            true);
        return;
    }

    const auto helperVenv = mrt::plugin::defaultWeightHelperVenvDir();
    const juce::StringArray args = mrt::plugin::buildWeightDownloadProcessArgs (
        helperScript,
        helperVenv,
        mlxfnToggle.getToggleState());

    weightDownloadOutput_.clear();
    if (! weightDownloadProcess_.start (args))
    {
        setStatusMessage (
            "Could not start weight helper bootstrap. Install Python 3 with venv support, "
            "or set MRT_JUCE_PYTHON=/path/to/python3.\nVenv: "
                + helperVenv.getFullPathName(),
            true);
        return;
    }

    weightDownloadRunning_ = true;
    downloadWeightsButton.setEnabled (false);
    setStatusMessage (
        "Preparing isolated weight-helper venv, then downloading weights into Hugging Face cache...\n"
            + args.joinIntoString (" "),
        true);
}

void PluginEditor::pollWeightDownload()
{
    if (! weightDownloadRunning_)
        return;

    char buffer[4096] {};
    for (;;)
    {
        const int n = weightDownloadProcess_.readProcessOutput (buffer, static_cast<int> (sizeof (buffer) - 1));
        if (n <= 0)
            break;
        buffer[n] = '\0';
        weightDownloadOutput_ += juce::String::fromUTF8 (buffer, n);
    }

    if (weightDownloadProcess_.isRunning())
    {
        if (weightDownloadOutput_.isNotEmpty())
            setStatusMessage (
                "Preparing/downloading weights using isolated helper venv...\n" + weightDownloadOutput_,
                false);
        return;
    }

    weightDownloadOutput_ += weightDownloadProcess_.readAllProcessOutput();
    const auto exitCode = weightDownloadProcess_.getExitCode();
    weightDownloadRunning_ = false;
    downloadWeightsButton.setEnabled (true);

    if (exitCode != 0)
    {
        setStatusMessage (
            "Weight download failed (exit " + juce::String (static_cast<int> (exitCode)) + ").\n"
                + weightDownloadOutput_,
            true);
        return;
    }

    const auto snapshotDir = extractSnapshotDir (weightDownloadOutput_);
    if (snapshotDir.isEmpty())
    {
        setStatusMessage (
            "Weight download finished, but the helper did not report SNAPSHOT_DIR.\n"
                + weightDownloadOutput_,
            true);
        return;
    }

    weightsEditor.setText (snapshotDir, juce::dontSendNotification);
    setStatusMessage ("Weights ready at:\n" + snapshotDir + "\n\n" + weightDownloadOutput_, true);
}

void PluginEditor::refreshRtfStatusDisplay()
{
    const auto rtf = processorRef.engine().generationRtfStats();
    rtfStatusLabel.setText (
        formatRtfStatusTextForDisplay (
            rtf,
            processorRef.downtempoMode(),
            static_cast<int> (std::lround (chunkLengthSlider.getValue()))),
        juce::dontSendNotification);
}

void PluginEditor::timerCallback()
{
    refreshRtfStatusDisplay();
    pollWeightDownload();
    publishEffectiveStyleCards();
    if (weightDownloadRunning_)
        return;

    const bool running = processorRef.engine().isRunning();
    const juce::String err (processorRef.engine().lastError());

    if (running)
    {
        if (err.isNotEmpty())
        {
            setStatusMessage ("Error: " + err, true);
            return;
        }

        const auto queued = processorRef.generatedAudioQueue().queuedFrames();
        const auto pre = processorRef.engine().prebufferTargetFrames();

        if (pre > 0 && queued < pre)
        {
            setRunningStatusMessage (mrt::plugin::RunningStatus::Prebuffering);
        }
        else
        {
            setRunningStatusMessage (mrt::plugin::RunningStatus::Playing);
        }

        return;
    }

    if (err.isNotEmpty())
    {
        const auto current = statusEditor.getText();
        if (current.startsWith ("Starting generation...")
            || current.contains ("Prebuffering:")
            || current.startsWith ("Playing"))
            setStatusMessage ("Stopped: " + err, true);
    }
}
