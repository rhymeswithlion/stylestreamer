#include "helpers/test_helpers.h"

#include "engine/MagentaRtJuceEngine.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <vector>

TEST_CASE ("PluginProcessor outputs generated audio from the queue", "[processor-audio]")
{
    PluginProcessor processor;
    processor.setDowntempoMode (false);
    processor.prepareToPlay (48000.0, 4);

    const float generated[] {
        0.1f, 0.2f,
        0.3f, 0.4f,
        0.5f, 0.6f,
        0.7f, 0.8f,
    };
    REQUIRE (processor.generatedAudioQueue().pushInterleaved (generated, 4, 2));

    juce::AudioBuffer<float> buffer (2, 4);
    juce::MidiBuffer midi;
    buffer.clear();

    processor.processBlock (buffer, midi);

    CHECK (buffer.getSample (0, 0) == Catch::Approx (0.1f));
    CHECK (buffer.getSample (0, 3) == Catch::Approx (0.7f));
    CHECK (buffer.getSample (1, 0) == Catch::Approx (0.2f));
    CHECK (buffer.getSample (1, 3) == Catch::Approx (0.8f));
}

TEST_CASE ("PluginProcessor outputs silence when generated queue underruns", "[processor-audio]")
{
    PluginProcessor processor;
    processor.setDowntempoMode (false);
    processor.prepareToPlay (48000.0, 4);

    juce::AudioBuffer<float> buffer (2, 4);
    juce::MidiBuffer midi;
    buffer.setSample (0, 0, 1.0f);
    buffer.setSample (1, 0, 1.0f);

    processor.processBlock (buffer, midi);

    CHECK (buffer.getSample (0, 0) == Catch::Approx (0.0f));
    CHECK (buffer.getSample (1, 0) == Catch::Approx (0.0f));
}

TEST_CASE ("PluginProcessor stop flushes queued generated audio", "[processor-audio]")
{
    PluginProcessor processor;
    processor.setDowntempoMode (false);
    processor.prepareToPlay (48000.0, 4);

    const float generated[] {
        0.1f, 0.2f,
        0.3f, 0.4f,
        0.5f, 0.6f,
        0.7f, 0.8f,
    };
    REQUIRE (processor.generatedAudioQueue().pushInterleaved (generated, 4, 2));

    processor.stopGeneration();

    juce::AudioBuffer<float> buffer (2, 4);
    juce::MidiBuffer midi;
    buffer.clear();

    processor.processBlock (buffer, midi);

    CHECK (buffer.getSample (0, 0) == Catch::Approx (0.0f));
    CHECK (buffer.getSample (0, 3) == Catch::Approx (0.0f));
    CHECK (buffer.getSample (1, 0) == Catch::Approx (0.0f));
    CHECK (buffer.getSample (1, 3) == Catch::Approx (0.0f));
}

TEST_CASE ("PluginProcessor prebuffer delays draining the queue", "[processor-audio]")
{
    PluginProcessor processor;
    processor.setDowntempoMode (false);
    processor.prepareToPlay (48000.0, 64);

    mrt::plugin::EngineSettings engineSettings;
    engineSettings.prebufferChunks = 4;
    processor.engine().configure (engineSettings);

    const std::size_t target = processor.engine().prebufferTargetFrames();
    REQUIRE (target > 1000);

    std::vector<float> interleaved (static_cast<std::size_t> (64 * 2));
    for (int i = 0; i < 64; ++i)
    {
        interleaved[static_cast<std::size_t> (i) * 2] = 0.4f;
        interleaved[static_cast<std::size_t> (i) * 2 + 1] = 0.5f;
    }

    for (int i = 0; i < 100; ++i)
        REQUIRE (processor.generatedAudioQueue().pushInterleaved (
            interleaved.data(), 64, 2));

    juce::AudioBuffer<float> buffer (2, 64);
    juce::MidiBuffer midi;

    processor.processBlock (buffer, midi);

    CHECK (buffer.getSample (0, 0) == Catch::Approx (0.0f));
    CHECK (processor.generatedAudioQueue().queuedFrames()
           > static_cast<std::size_t> (buffer.getNumSamples()));

    mrt::plugin::LiveControlSnapshot live = processor.engine().liveControls();
    live.prebufferChunks = 0;
    processor.engine().setLiveControls (std::move (live));

    processor.processBlock (buffer, midi);

    CHECK (buffer.getSample (0, 0) == Catch::Approx (0.4f));
    CHECK (buffer.getSample (1, 0) == Catch::Approx (0.5f));
}

TEST_CASE ("PluginProcessor prebuffer is a one-shot playback latch", "[processor-audio]")
{
    PluginProcessor processor;
    processor.setDowntempoMode (false);
    processor.prepareToPlay (48000.0, 64);

    mrt::plugin::EngineSettings engineSettings;
    engineSettings.prebufferChunks = 1;
    processor.engine().configure (engineSettings);

    const std::size_t target = processor.engine().prebufferTargetFrames();
    REQUIRE (target > 64);

    std::vector<float> interleaved (target * 2);
    for (std::size_t i = 0; i < target; ++i)
    {
        interleaved[i * 2] = 0.25f;
        interleaved[i * 2 + 1] = 0.75f;
    }
    REQUIRE (processor.generatedAudioQueue().pushInterleaved (
        interleaved.data(), target, 2));

    juce::AudioBuffer<float> buffer (2, 64);
    juce::MidiBuffer midi;

    processor.processBlock (buffer, midi);
    CHECK (buffer.getSample (0, 0) == Catch::Approx (0.25f));
    CHECK (buffer.getSample (1, 0) == Catch::Approx (0.75f));
    REQUIRE (processor.generatedAudioQueue().queuedFrames() < target);

    // This second callback used to mute because queuedFrames dropped below the
    // prebuffer target after the first block. Once playback is armed it should
    // continue draining until an actual underrun.
    processor.processBlock (buffer, midi);
    CHECK (buffer.getSample (0, 0) == Catch::Approx (0.25f));
    CHECK (buffer.getSample (1, 0) == Catch::Approx (0.75f));
}

TEST_CASE ("PluginProcessor starts in downtempo mode and remains live-toggleable", "[processor-audio]")
{
    PluginProcessor processor;

    CHECK (processor.downtempoMode());
    CHECK (processor.currentDowntempoSpeedRatio() == Catch::Approx (32000.0 / 44100.0).epsilon (0.001));

    processor.setDowntempoMode (false);
    CHECK_FALSE (processor.downtempoMode());

    processor.setDowntempoMode (true);
    CHECK (processor.downtempoMode());
}

TEST_CASE ("PluginProcessor downtempo drains generated audio slower than realtime", "[processor-audio]")
{
    PluginProcessor normal;
    constexpr int blockFrames = 4800;
    normal.setDowntempoMode (false);
    normal.prepareToPlay (48000.0, blockFrames);
    PluginProcessor downtempo;
    downtempo.prepareToPlay (48000.0, blockFrames);
    downtempo.setDowntempoMode (true);

    std::vector<float> interleaved (48000 * 2);
    for (int frame = 0; frame < 48000; ++frame)
    {
        interleaved[static_cast<std::size_t> (frame) * 2] = 0.25f;
        interleaved[static_cast<std::size_t> (frame) * 2 + 1] = 0.75f;
    }

    REQUIRE (normal.generatedAudioQueue().pushInterleaved (interleaved.data(), 48000, 2));
    REQUIRE (downtempo.generatedAudioQueue().pushInterleaved (interleaved.data(), 48000, 2));

    juce::AudioBuffer<float> normalBuffer (2, blockFrames);
    juce::AudioBuffer<float> downtempoBuffer (2, blockFrames);
    juce::MidiBuffer midi;

    for (int i = 0; i < 4; ++i)
    {
        normal.processBlock (normalBuffer, midi);
        downtempo.processBlock (downtempoBuffer, midi);
    }

    CHECK (normal.generatedAudioQueue().queuedFrames() == 28800);
    CHECK (downtempo.generatedAudioQueue().queuedFrames() > normal.generatedAudioQueue().queuedFrames());
    CHECK (downtempoBuffer.getRMSLevel (0, 0, downtempoBuffer.getNumSamples()) > 0.0f);
}

TEST_CASE ("PluginProcessor downtempo speed changes with a 0.4 second sigmoid ramp", "[processor-audio]")
{
    PluginProcessor processor;
    constexpr int blockFrames = 4800;
    processor.setDowntempoMode (false);
    processor.prepareToPlay (48000.0, blockFrames);

    std::vector<float> interleaved (48000 * 2, 0.5f);
    REQUIRE (processor.generatedAudioQueue().pushInterleaved (interleaved.data(), 48000, 2));

    juce::AudioBuffer<float> buffer (2, blockFrames);
    juce::MidiBuffer midi;

    CHECK (processor.currentDowntempoSpeedRatio() == Catch::Approx (1.0));

    processor.setDowntempoMode (true);
    CHECK (processor.currentDowntempoSpeedRatio() == Catch::Approx (1.0));

    processor.processBlock (buffer, midi);
    processor.processBlock (buffer, midi);
    CHECK (processor.currentDowntempoSpeedRatio() < 0.9);
    CHECK (processor.currentDowntempoSpeedRatio() > 0.75);

    processor.processBlock (buffer, midi);
    processor.processBlock (buffer, midi);
    CHECK (processor.currentDowntempoSpeedRatio() == Catch::Approx (32000.0 / 44100.0).epsilon (0.001));
}

TEST_CASE ("PluginProcessor downtempo speed ramps back to realtime", "[processor-audio]")
{
    PluginProcessor processor;
    constexpr int blockFrames = 4800;
    processor.prepareToPlay (48000.0, blockFrames);

    std::vector<float> interleaved (96000 * 2, 0.5f);
    REQUIRE (processor.generatedAudioQueue().pushInterleaved (interleaved.data(), 96000, 2));

    juce::AudioBuffer<float> buffer (2, blockFrames);
    juce::MidiBuffer midi;

    processor.setDowntempoMode (true);
    for (int i = 0; i < 4; ++i)
        processor.processBlock (buffer, midi);
    REQUIRE (processor.currentDowntempoSpeedRatio() == Catch::Approx (32000.0 / 44100.0).epsilon (0.001));

    processor.setDowntempoMode (false);
    CHECK (processor.currentDowntempoSpeedRatio() == Catch::Approx (32000.0 / 44100.0).epsilon (0.001));

    processor.processBlock (buffer, midi);
    processor.processBlock (buffer, midi);
    CHECK (processor.currentDowntempoSpeedRatio() > 0.75);
    CHECK (processor.currentDowntempoSpeedRatio() < 0.95);

    processor.processBlock (buffer, midi);
    processor.processBlock (buffer, midi);
    CHECK (processor.currentDowntempoSpeedRatio() == Catch::Approx (1.0).epsilon (0.001));
}
