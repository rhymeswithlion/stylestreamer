#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "engine/MagentaRtJuceEngine.h"
#include "engine/GeneratedAudioQueue.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>

namespace
{

void touchWeightFile (const std::filesystem::path& path)
{
    std::filesystem::create_directories (path.parent_path());
    std::ofstream out (path);
    out << "x";
}

std::filesystem::path tempWeightsDir (std::string_view name)
{
    const auto root = std::filesystem::temp_directory_path()
        / ("stylestreamer-engine-tests-" + std::string (name));
    std::filesystem::remove_all (root);
    std::filesystem::create_directories (root);
    return root;
}

void addRequiredWeightFiles (const std::filesystem::path& root)
{
    touchWeightFile (root / "spectrostream_encoder.safetensors");
    touchWeightFile (root / "spectrostream_decoder.safetensors");
    touchWeightFile (root / "spectrostream_codebooks.safetensors");
    touchWeightFile (root / "musiccoca_encoder.safetensors");
    touchWeightFile (root / "musiccoca_codebooks.safetensors");
    touchWeightFile (root / "musiccoca_vocab.model");
    touchWeightFile (root / "depthformer" / "depthformer_base.safetensors");
}

} // namespace

TEST_CASE ("MagentaRtJuceEngine defaults to 400 ms chunks", "[engine]")
{
    mrt::plugin::MagentaRtJuceEngine engine;
    mrt::plugin::EngineSettings settings;

    engine.configure (settings);

    CHECK (settings.chunkLengthSeconds == Catch::Approx (0.4f));
    CHECK (engine.runtimeSettings().chunkLengthSeconds == Catch::Approx (0.4f));
}

TEST_CASE ("MagentaRtJuceEngine defaults style transition delay to four seconds", "[engine]")
{
    mrt::plugin::MagentaRtJuceEngine engine;
    mrt::plugin::EngineSettings settings;

    engine.configure (settings);

    CHECK (engine.liveControls().styleTransitionDelaySeconds == Catch::Approx (4.0));
}

TEST_CASE ("MagentaRtJuceEngine maps settings to MLX chunk options", "[engine]")
{
    mrt::plugin::MagentaRtJuceEngine engine;
    mrt::plugin::EngineSettings settings;
    settings.weightsDirectory = "/base/weights";
    settings.depthformerWeightsPath = "/fine-tuned/depthformer.safetensors";
    settings.temperature = 1.35f;
    settings.topK = 128;
    settings.guidanceWeight = 6.5f;
    settings.seed = 9000;
    settings.speculative = true;
    settings.pipelineEncoder = true;
    settings.streamingCached = true;
    settings.encoderRefreshFrames = 5;
    settings.chunkLengthSeconds = 0.04f;

    engine.configure (settings);

    const auto runtime = engine.runtimeSettings();
    CHECK (runtime.weightsDirectory == std::filesystem::path ("/base/weights"));
    CHECK (runtime.depthformerWeightsPath == std::filesystem::path ("/fine-tuned/depthformer.safetensors"));
    CHECK (runtime.chunkLengthSeconds == Catch::Approx (0.04f));

    const auto options = engine.makeGenerateOptions (3);

    CHECK (options.temperature == Catch::Approx (1.35f));
    CHECK (options.top_k == 128);
    CHECK (options.guidance_weight == Catch::Approx (6.5f));
    REQUIRE (options.seed.has_value());
    CHECK (*options.seed == 9003);
    CHECK (options.speculative);
    CHECK (options.pipeline_encoder);
    CHECK (options.streaming_cached);
    CHECK (options.encoder_refresh_frames == 5);
}

TEST_CASE ("MagentaRtJuceEngine makeGenerateOptions uses explicit live snapshot", "[engine]")
{
    mrt::plugin::MagentaRtJuceEngine engine;
    mrt::plugin::EngineSettings settings;
    settings.temperature = 1.0f;
    settings.topK = 40;
    settings.guidanceWeight = 5.0f;
    settings.seed = 100;
    engine.configure (settings);

    mrt::plugin::LiveControlSnapshot alt;
    alt.temperature = 2.5f;
    alt.topK = 10;
    alt.guidanceWeight = 1.0f;
    alt.seed = 7;
    alt.streamingCached = true;
    alt.encoderRefreshFrames = 10;

    const auto opts = engine.makeGenerateOptions (2, alt);

    CHECK (opts.temperature == Catch::Approx (2.5f));
    CHECK (opts.top_k == 10);
    CHECK (opts.guidance_weight == Catch::Approx (1.0f));
    REQUIRE (opts.seed.has_value());
    CHECK (*opts.seed == 9);
    CHECK (opts.streaming_cached);
    CHECK (opts.encoder_refresh_frames == 10);
}

TEST_CASE ("MagentaRtJuceEngine setLiveControls updates sampler without configure", "[engine]")
{
    mrt::plugin::MagentaRtJuceEngine engine;
    mrt::plugin::EngineSettings settings;
    settings.temperature = 1.0f;
    settings.topK = 40;
    engine.configure (settings);

    mrt::plugin::LiveControlSnapshot next = engine.liveControls();
    next.temperature = 0.25f;
    next.topK = 256;
    engine.setLiveControls (std::move (next));

    const auto options = engine.makeGenerateOptions (0);
    CHECK (options.temperature == Catch::Approx (0.25f));
    CHECK (options.top_k == 256);
}

TEST_CASE ("MagentaRtJuceEngine exposes deterministic prompt selection snapshot", "[engine]")
{
    mrt::plugin::MagentaRtJuceEngine engine;
    mrt::plugin::PromptPortfolio portfolio (2);
    portfolio.setSlot (0, "ambient synths", 0.5f);
    portfolio.setSlot (1, "driving techno", 1.0f);

    engine.setPromptPortfolio (portfolio);
    const auto selection = engine.currentPromptSelection();

    REQUIRE (selection.prompt.has_value());
    CHECK (*selection.prompt == "ambient synths");
    CHECK (selection.activeCount == 2);
    CHECK (selection.totalWeight == Catch::Approx (1.5f));
}

TEST_CASE ("MagentaRtJuceEngine starts unloaded and reset keeps it stopped", "[engine]")
{
    mrt::plugin::MagentaRtJuceEngine engine;

    CHECK_FALSE (engine.isLoaded());
    CHECK_FALSE (engine.isRunning());

    engine.reset();

    CHECK_FALSE (engine.isLoaded());
    CHECK_FALSE (engine.isRunning());
}

TEST_CASE ("MagentaRtJuceEngine converts configured MLX dtype", "[engine]")
{
    mrt::plugin::MagentaRtJuceEngine engine;
    mrt::plugin::EngineSettings settings;
    settings.dtype = mrt::plugin::ComputeDtype::Float32;

    engine.configure (settings);

    CHECK (engine.mlxDtype() == mlx::core::float32);
}

TEST_CASE ("MagentaRtJuceEngine reports resolved weight paths including Depthformer override", "[engine]")
{
    const auto weights = tempWeightsDir ("resolved-paths");
    addRequiredWeightFiles (weights);
    const auto overridePath = weights / "finetuned" / "dnb3-depthformer-final.safetensors";
    touchWeightFile (overridePath);

    mrt::plugin::MagentaRtJuceEngine engine;
    mrt::plugin::EngineSettings settings;
    settings.weightsDirectory = weights;
    settings.depthformerWeightsPath = overridePath;
    engine.configure (settings);

    const auto status = engine.resolveConfiguredWeightPathsStatus();

    CHECK (status.find ("SpectroStream encoder: " + (weights / "spectrostream_encoder.safetensors").string()) != std::string::npos);
    CHECK (status.find ("SpectroStream decoder: " + (weights / "spectrostream_decoder.safetensors").string()) != std::string::npos);
    CHECK (status.find ("SpectroStream codebooks: " + (weights / "spectrostream_codebooks.safetensors").string()) != std::string::npos);
    CHECK (status.find ("MusicCoCa encoder: " + (weights / "musiccoca_encoder.safetensors").string()) != std::string::npos);
    CHECK (status.find ("MusicCoCa codebooks: " + (weights / "musiccoca_codebooks.safetensors").string()) != std::string::npos);
    CHECK (status.find ("MusicCoCa vocab: " + (weights / "musiccoca_vocab.model").string()) != std::string::npos);
    CHECK (status.find ("Depthformer checkpoint: " + overridePath.string()) != std::string::npos);
}

TEST_CASE ("MagentaRtJuceEngine reports background load errors without touching audio queue", "[engine]")
{
    mrt::plugin::MagentaRtJuceEngine engine;
    mrt::plugin::EngineSettings settings;
    settings.weightsDirectory = "/path/that/does/not/exist";
    engine.configure (settings);

    mrt::plugin::GeneratedAudioQueue queue (2, 8);
    engine.start (queue);

    for (int attempt = 0; attempt < 100 && engine.isRunning(); ++attempt)
        std::this_thread::sleep_for (std::chrono::milliseconds (10));

    CHECK_FALSE (engine.isRunning());
    CHECK_FALSE (engine.lastError().empty());
    CHECK (queue.queuedFrames() == 0);
}
