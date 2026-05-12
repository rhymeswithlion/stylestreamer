#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <algorithm>
#include <cmath>

//==============================================================================
PluginProcessor::PluginProcessor()
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
{
}

PluginProcessor::~PluginProcessor()
{
    stopGeneration();
}

//==============================================================================
const juce::String PluginProcessor::getName() const
{
    return JucePlugin_Name;
}

bool PluginProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool PluginProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool PluginProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double PluginProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int PluginProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int PluginProcessor::getCurrentProgram()
{
    return 0;
}

void PluginProcessor::setCurrentProgram (int index)
{
    juce::ignoreUnused (index);
}

const juce::String PluginProcessor::getProgramName (int index)
{
    juce::ignoreUnused (index);
    return {};
}

void PluginProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}

//==============================================================================
void PluginProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback
    // initialisation that you need..
    juce::ignoreUnused (samplesPerBlock);
    playbackSampleRate_ = sampleRate > 0.0 ? sampleRate : generatedAudioRate;
    downtempoRampTotalSamples_ = std::max (1.0, downtempoRampSeconds * playbackSampleRate_);
    downtempoRampProgressSamples_ = downtempoRampTotalSamples_;
    const double preparedSpeedRatio =
        downtempoMode_.load (std::memory_order_acquire) ? downtempoSpeedRatio : 1.0;
    currentDowntempoSpeedRatio_ = preparedSpeedRatio;
    rampStartSpeedRatio_ = preparedSpeedRatio;
    targetDowntempoSpeedRatio_ = preparedSpeedRatio;
    playbackArmed_.store (false, std::memory_order_release);
    resetDowntempoResamplers();
    generatedAudioQueue_.clear();
}

void PluginProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

bool PluginProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}

void PluginProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                              juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);

    juce::ScopedNoDenormals noDenormals;
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    std::array<float*, 64> outputPointers {};
    const auto outputChannels =
        std::min (totalNumOutputChannels, static_cast<int> (outputPointers.size()));

    for (int channel = 0; channel < outputChannels; ++channel)
        outputPointers[static_cast<std::size_t> (channel)] = buffer.getWritePointer (channel);

    const auto preTarget = engine_.prebufferTargetFrames();
    if (preTarget > 0 && ! playbackArmed_.load (std::memory_order_acquire))
    {
        if (generatedAudioQueue_.queuedFrames() < preTarget)
        {
            buffer.clear();
            return;
        }

        // Match mlx-stream: prebuffer gates the transition into playback once,
        // not every callback. Otherwise the first drained block drops the queue
        // below the threshold and the next callback mutes, causing a sawtooth.
        playbackArmed_.store (true, std::memory_order_release);
    }

    const double desiredSpeedRatio =
        downtempoMode_.load (std::memory_order_acquire) ? downtempoSpeedRatio : 1.0;
    if (std::abs (targetDowntempoSpeedRatio_ - desiredSpeedRatio) > 1.0e-9)
        beginDowntempoRamp (desiredSpeedRatio);

    const bool needsDowntempoProcessing =
        downtempoResamplerActive_
        || downtempoMode_.load (std::memory_order_acquire)
        || downtempoRampProgressSamples_ < downtempoRampTotalSamples_
        || std::abs (currentDowntempoSpeedRatio_ - 1.0) > 1.0e-9;

    std::size_t copied = 0;
    if (needsDowntempoProcessing)
    {
        processDowntempoBlock (buffer, outputChannels);
        copied = static_cast<std::size_t> (buffer.getNumSamples());
    }
    else
    {
        copied = generatedAudioQueue_.popToDeinterleaved (
            outputPointers.data(), outputChannels, static_cast<std::size_t> (buffer.getNumSamples()));
    }

    if (preTarget > 0 && engine_.isRunning()
        && copied < static_cast<std::size_t> (buffer.getNumSamples()))
    {
        playbackArmed_.store (false, std::memory_order_release);
        resetDowntempoResamplers();
    }

    for (int channel = outputChannels; channel < totalNumOutputChannels; ++channel)
        buffer.clear (channel, 0, buffer.getNumSamples());
}

//==============================================================================
bool PluginProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new PluginEditor (*this);
}

//==============================================================================
void PluginProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
    juce::ignoreUnused (destData);
}

void PluginProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
    juce::ignoreUnused (data, sizeInBytes);
}

mrt::plugin::GeneratedAudioQueue& PluginProcessor::generatedAudioQueue() noexcept
{
    return generatedAudioQueue_;
}

const mrt::plugin::GeneratedAudioQueue& PluginProcessor::generatedAudioQueue() const noexcept
{
    return generatedAudioQueue_;
}

mrt::plugin::MagentaRtJuceEngine& PluginProcessor::engine() noexcept
{
    return engine_;
}

const mrt::plugin::MagentaRtJuceEngine& PluginProcessor::engine() const noexcept
{
    return engine_;
}

void PluginProcessor::startGeneration()
{
    playbackArmed_.store (false, std::memory_order_release);
    engine_.start (generatedAudioQueue_);
}

void PluginProcessor::stopGeneration()
{
    engine_.stop();
    playbackArmed_.store (false, std::memory_order_release);
    resetDowntempoResamplers();
    generatedAudioQueue_.clear();
}

void PluginProcessor::resetGeneration()
{
    playbackArmed_.store (false, std::memory_order_release);
    resetDowntempoResamplers();
    generatedAudioQueue_.clear();
    engine_.reset();
}

void PluginProcessor::setDowntempoMode (bool enabled) noexcept
{
    downtempoMode_.store (enabled, std::memory_order_release);
}

bool PluginProcessor::downtempoMode() const noexcept
{
    return downtempoMode_.load (std::memory_order_acquire);
}

double PluginProcessor::currentDowntempoSpeedRatio() const noexcept
{
    return currentDowntempoSpeedRatio_;
}

void PluginProcessor::resetDowntempoResamplers() noexcept
{
    for (auto& interpolator : downtempoInterpolators_)
        interpolator.reset();
    downtempoResamplerActive_ = false;
}

void PluginProcessor::beginDowntempoRamp (double targetRatio) noexcept
{
    rampStartSpeedRatio_ = currentDowntempoSpeedRatio_;
    targetDowntempoSpeedRatio_ = targetRatio;
    downtempoRampProgressSamples_ = 0.0;
    downtempoRampTotalSamples_ = std::max (1.0, downtempoRampSeconds * playbackSampleRate_);
}

double PluginProcessor::advanceDowntempoRamp (int numOutputSamples) noexcept
{
    if (downtempoRampProgressSamples_ >= downtempoRampTotalSamples_)
    {
        currentDowntempoSpeedRatio_ = targetDowntempoSpeedRatio_;
        return currentDowntempoSpeedRatio_;
    }

    downtempoRampProgressSamples_ =
        std::min (downtempoRampTotalSamples_,
            downtempoRampProgressSamples_ + static_cast<double> (std::max (0, numOutputSamples)));

    const double x = downtempoRampProgressSamples_ / downtempoRampTotalSamples_;
    const auto sigmoid = [] (double t) {
        constexpr double steepness = 12.0;
        return 1.0 / (1.0 + std::exp (-steepness * (t - 0.5)));
    };
    const double lo = sigmoid (0.0);
    const double hi = sigmoid (1.0);
    const double shaped = (sigmoid (x) - lo) / (hi - lo);

    currentDowntempoSpeedRatio_ =
        rampStartSpeedRatio_ + (targetDowntempoSpeedRatio_ - rampStartSpeedRatio_) * shaped;
    return currentDowntempoSpeedRatio_;
}

void PluginProcessor::processDowntempoBlock (juce::AudioBuffer<float>& buffer, int outputChannels)
{
    downtempoResamplerActive_ = true;
    const int outputSamples = buffer.getNumSamples();
    const double speedRatio = advanceDowntempoRamp (outputSamples);
    const int inputFramesNeeded = std::max (1,
        static_cast<int> (std::ceil (static_cast<double> (outputSamples) * speedRatio)));

    downtempoInputBuffer_.setSize (
        outputChannels,
        inputFramesNeeded,
        false,
        false,
        true);
    downtempoInputBuffer_.clear();

    std::array<float*, 64> inputPointers {};
    for (int channel = 0; channel < outputChannels; ++channel)
        inputPointers[static_cast<std::size_t> (channel)] =
            downtempoInputBuffer_.getWritePointer (channel);

    const auto copied = generatedAudioQueue_.popToDeinterleaved (
        inputPointers.data(),
        outputChannels,
        static_cast<std::size_t> (inputFramesNeeded));

    for (int channel = 0; channel < outputChannels; ++channel)
    {
        downtempoInterpolators_[static_cast<std::size_t> (channel)].process (
            speedRatio,
            downtempoInputBuffer_.getReadPointer (channel),
            buffer.getWritePointer (channel),
            outputSamples,
            inputFramesNeeded,
            0);
    }

    if (copied < static_cast<std::size_t> (inputFramesNeeded))
    {
        resetDowntempoResamplers();
        if (engine_.isRunning())
            playbackArmed_.store (false, std::memory_order_release);
    }
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}
