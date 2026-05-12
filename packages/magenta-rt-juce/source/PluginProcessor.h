#pragma once

#include "engine/GeneratedAudioQueue.h"
#include "engine/MagentaRtJuceEngine.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <atomic>

#if (MSVC)
#include "ipps.h"
#endif

class PluginProcessor : public juce::AudioProcessor
{
public:
    PluginProcessor();
    ~PluginProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    mrt::plugin::GeneratedAudioQueue& generatedAudioQueue() noexcept;
    const mrt::plugin::GeneratedAudioQueue& generatedAudioQueue() const noexcept;
    mrt::plugin::MagentaRtJuceEngine& engine() noexcept;
    const mrt::plugin::MagentaRtJuceEngine& engine() const noexcept;

    void startGeneration();
    void stopGeneration();
    void resetGeneration();
    void setDowntempoMode (bool enabled) noexcept;
    [[nodiscard]] bool downtempoMode() const noexcept;
    [[nodiscard]] double currentDowntempoSpeedRatio() const noexcept;

private:
    static constexpr double downtempoSourceRate = 32000.0;
    // SpectroStream's original output is 48 kHz; intentionally play those
    // samples as 44.1 kHz audio so they run slightly slower and lower in pitch.
    static constexpr double generatedAudioRate = 44100.0;
    static constexpr double downtempoSpeedRatio = downtempoSourceRate / generatedAudioRate;
    static constexpr double downtempoRampSeconds = 0.4;

    void resetDowntempoResamplers() noexcept;
    void processDowntempoBlock (juce::AudioBuffer<float>& buffer, int outputChannels);
    void beginDowntempoRamp (double targetRatio) noexcept;
    double advanceDowntempoRamp (int numOutputSamples) noexcept;

    // 24 generated chunks at the intentional 44.1 kHz playback rate. This
    // matches the Advanced max-queue slider ceiling; smaller buffers made high
    // prebuffer settings unreachable.
    mrt::plugin::GeneratedAudioQueue generatedAudioQueue_ { 2, 44100 * 2 * 24 };
    mrt::plugin::MagentaRtJuceEngine engine_;
    std::atomic<bool> playbackArmed_ { false };
    std::atomic<bool> downtempoMode_ { true };
    double playbackSampleRate_ { generatedAudioRate };
    double currentDowntempoSpeedRatio_ { downtempoSpeedRatio };
    double rampStartSpeedRatio_ { downtempoSpeedRatio };
    double targetDowntempoSpeedRatio_ { downtempoSpeedRatio };
    double downtempoRampProgressSamples_ { downtempoRampSeconds * generatedAudioRate };
    double downtempoRampTotalSamples_ { downtempoRampSeconds * generatedAudioRate };
    bool downtempoResamplerActive_ { false };
    std::array<juce::WindowedSincInterpolator, 64> downtempoInterpolators_;
    juce::AudioBuffer<float> downtempoInputBuffer_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
};
