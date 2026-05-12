#include <catch2/catch_test_macros.hpp>

#include "engine/GeneratedAudioQueue.h"
#include "engine/MagentaRtJuceEngine.h"
#include "PluginEditor.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <thread>

TEST_CASE ("MagentaRtJuceEngine generates one chunk with a complete MLX weights cache",
    "[smoke][slow][.]")
{
    const auto* weightsEnv = std::getenv ("MRT_JUCE_WEIGHTS_DIR");
    if (weightsEnv == nullptr || std::string_view (weightsEnv).empty())
        SKIP ("Set MRT_JUCE_WEIGHTS_DIR to a complete C++ MLX .weights-cache to run this smoke test");

    const std::filesystem::path weightsDir { weightsEnv };
    REQUIRE (std::filesystem::is_directory (weightsDir));

    mrt::plugin::MagentaRtJuceEngine engine;
    mrt::plugin::EngineSettings settings;
    settings.weightsDirectory = weightsDir;
    settings.seed = 0;
    settings.pipelineEncoder = true;
    engine.configure (settings);

    mrt::plugin::PromptPortfolio portfolio (1);
    portfolio.setSlot (0, "deep house", 1.0f);
    engine.setPromptPortfolio (std::move (portfolio));

    mrt::plugin::GeneratedAudioQueue queue (2, 48000 * 4);
    engine.start (queue);

    for (int attempt = 0; attempt < 400 && queue.queuedFrames() == 0 && engine.lastError().empty();
         ++attempt)
    {
        std::this_thread::sleep_for (std::chrono::milliseconds (50));
    }

    engine.stop();

    CHECK (engine.lastError().empty());
    CHECK (queue.queuedFrames() > 0);
}

TEST_CASE ("MagentaRtJuceEngine generates chunk with weighted prompt slots", "[smoke][slow][.]")
{
    const auto* weightsEnv = std::getenv ("MRT_JUCE_WEIGHTS_DIR");
    if (weightsEnv == nullptr || std::string_view (weightsEnv).empty())
        SKIP ("Set MRT_JUCE_WEIGHTS_DIR to a complete C++ MLX .weights-cache to run this smoke test");

    const std::filesystem::path weightsDir { weightsEnv };
    REQUIRE (std::filesystem::is_directory (weightsDir));

    mrt::plugin::MagentaRtJuceEngine engine;
    mrt::plugin::EngineSettings settings;
    settings.weightsDirectory = weightsDir;
    settings.seed = 0;
    settings.pipelineEncoder = true;
    engine.configure (settings);

    mrt::plugin::PromptPortfolio portfolio (2);
    portfolio.setSlot (0, "deep house", 0.7f);
    portfolio.setSlot (1, "ambient pads", 0.3f);
    engine.setPromptPortfolio (std::move (portfolio));

    mrt::plugin::GeneratedAudioQueue queue (2, 48000 * 4);
    engine.start (queue);

    for (int attempt = 0; attempt < 400 && queue.queuedFrames() == 0 && engine.lastError().empty();
         ++attempt)
    {
        std::this_thread::sleep_for (std::chrono::milliseconds (50));
    }

    engine.stop();

    CHECK (engine.lastError().empty());
    CHECK (queue.queuedFrames() > 0);
}

TEST_CASE ("StyleStreamer style cards keep WebView disabled", "[juce][style-cards]")
{
    CHECK (JUCE_WEB_BROWSER == 0);
    CHECK (JUCE_USE_CURL == 0);
}

TEST_CASE ("PluginEditor formats compact RTF header stats with chunk and chunk size", "[juce][ui]")
{
    mrt::plugin::GenerationRtfStats stats;
    stats.lastChunkRtf = 1.4;
    stats.averageRtf = 1.3;
    stats.completedChunks = 60;

    const auto label = formatRtfStatusTextForDisplay (stats, false, 400);

    CHECK (label.contains ("last 1.40x"));
    CHECK (label.contains ("avg 1.30x"));
    CHECK (label.contains ("chunk 60"));
    CHECK (label.contains ("chunk size 400ms"));
}
