#pragma once

#include "PluginProcessor.h"
#include "BinaryData.h"
#include "engine/PromptStateClipboard.h"
#include "engine/StyleCardDeck.h"
#include "engine/StatusLog.h"
#include "melatonin_inspector/melatonin_inspector.h"
#include "ui/StyleCardsPanel.h"

#include <array>
#include <memory>

class StyleStreamerLookAndFeel : public juce::LookAndFeel_V4
{
public:
    StyleStreamerLookAndFeel();

    void drawToggleButton (juce::Graphics&,
        juce::ToggleButton&,
        bool shouldDrawButtonAsHighlighted,
        bool shouldDrawButtonAsDown) override;
};

//==============================================================================
class AdvancedOptionsContent;
class StyleStreamerAdvancedWindow;

[[nodiscard]] juce::String formatRtfStatusTextForDisplay (
    const mrt::plugin::GenerationRtfStats& stats,
    bool downtempoMode,
    int chunkLengthMs);

class PluginEditor : public juce::AudioProcessorEditor,
                     private juce::Timer
{
public:
    explicit PluginEditor (PluginProcessor&);
    ~PluginEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

    /** Closes the floating advanced-options window if open (safe from async). */
    void closeAdvancedOptionsWindow();
    [[nodiscard]] bool hasStyleCardsSurfaceForTesting() const noexcept { return styleCardsPanel_.isVisible(); }
    [[nodiscard]] mrt::plugin::PromptStateClipboardPayload promptStatePayloadForTesting() const
    {
        return makePromptStatePayloadFromUi();
    }
    void applyPromptStatePayloadForTesting (const mrt::plugin::PromptStateClipboardPayload& payload)
    {
        applyPromptStatePayloadToUi (payload);
    }

private:
    friend class AdvancedOptionsContent;

    void timerCallback() override;
    static constexpr int promptSlotCount = 4;

    void configureControls();
    void loadDistillPromptPool();
    void applyControlsToProcessor();
    void applyLiveControlsFromUi();
    void applyPromptPortfolioFromUi();
    void handleStyleCardEvent (const mrt::plugin::StyleCardEvent& event);
    void publishEffectiveStyleCards();
    [[nodiscard]] mrt::plugin::PromptStateClipboardPayload makePromptStatePayloadFromUi() const;
    void applyPromptStatePayloadToUi (const mrt::plugin::PromptStateClipboardPayload& payload);
    void setStatusMessage (const juce::String& message, bool scrollToTop);
    [[nodiscard]] juce::String loadedWeightsStatusBlock() const;
    void setRunningStatusMessage (mrt::plugin::RunningStatus status);
    void refreshRtfStatusDisplay();
    void startWeightDownload();
    void pollWeightDownload();
    juce::Rectangle<int> layoutRow (juce::Rectangle<int>& area, int height, int gap = 8);

    [[nodiscard]] bool advancedControlsOnMainEditor() const noexcept;
    void reparentAdvancedControlsTo (juce::Component& host);
    void layoutAdvancedControlsIn (juce::Rectangle<int> bounds);
    void openAdvancedOptionsWindow();
    void hideAdvancedWidgetsAfterReparentToEditor();

    bool advancedWidgetsShownOnMainEditor_ { false };

    std::unique_ptr<StyleStreamerAdvancedWindow> advancedOptionsWindow_;

    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    PluginProcessor& processorRef;
    std::unique_ptr<melatonin::Inspector> inspector;

    StyleStreamerLookAndFeel styleLaf_;
    juce::Image heroImage {};
    juce::StringArray distillPromptPool;
    mrt::plugin::StyleCardDeck styleCardDeck_;
    mrt::plugin::StyleCardsPanel styleCardsPanel_;
    double lastStyleCardsPublishSeconds_ = 0.0;

    std::array<juce::Label, promptSlotCount> promptLabels;
    std::array<juce::TextEditor, promptSlotCount> promptEditors;
    std::array<juce::Slider, promptSlotCount> promptWeights;

    juce::Label weightsLabel;
    juce::TextEditor weightsEditor;
    juce::Label depthformerWeightsLabel;
    juce::TextEditor depthformerWeightsEditor;
    juce::Label seedLabel;
    juce::TextEditor seedEditor;
    juce::Label temperatureLabel;
    juce::Slider temperatureSlider;
    juce::Label topKLabel;
    juce::Slider topKSlider;
    juce::Label guidanceLabel;
    juce::Slider guidanceSlider;
    juce::Label guidanceHint;
    juce::Label prebufferLabel;
    juce::Slider prebufferSlider;
    juce::Label maxQueueLabel;
    juce::Slider maxQueueSlider;
    juce::Label chunkLengthLabel;
    juce::Slider chunkLengthSlider;
    juce::Label styleTransitionDelayLabel;
    juce::Slider styleTransitionDelaySlider;
    juce::Label dtypeLabel;
    juce::ComboBox dtypeBox;
    juce::ToggleButton mlxfnToggle { "Use MLXFN fast paths" };

    juce::TextButton startButton { "Start" };
    juce::TextButton stopButton { "Stop" };
    juce::TextButton resetButton { "Reset" };
    juce::Label rtfStatusLabel;
    juce::TextEditor statusEditor;
    mrt::plugin::StatusLog statusLog_;

    juce::TextButton inspectButton { "Inspect UI" };
    juce::TextButton copyStatusButton { "Copy log" };
    juce::TextButton copyStateButton { "Copy state" };
    juce::TextButton pasteStateButton { "Paste state" };
    juce::TextButton downloadWeightsButton { "Download weights" };
    juce::ToggleButton downtempoToggle { "Downtempo" };
    juce::TextButton advancedOptionsButton { "Advanced..." };
    juce::ChildProcess weightDownloadProcess_;
    bool weightDownloadRunning_ { false };
    juce::String weightDownloadOutput_;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
