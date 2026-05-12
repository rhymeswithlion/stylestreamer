#include "engine/MagentaRtJuceEngine.h"
#include "engine/MlxfnEnv.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace mrt::plugin
{
namespace
{

[[nodiscard]] std::string formatWeightPaths (
    const magenta_realtime_mlx::InferenceBundlePaths& paths)
{
    std::ostringstream out;
    out << "SpectroStream encoder: " << paths.spectrostream_encoder.string() << "\n";
    out << "SpectroStream decoder: " << paths.spectrostream_decoder.string() << "\n";
    out << "SpectroStream codebooks: " << paths.spectrostream_codebooks.string() << "\n";
    out << "MusicCoCa encoder: " << paths.musiccoca_encoder.string() << "\n";
    out << "MusicCoCa codebooks: " << paths.musiccoca_codebooks.string() << "\n";
    out << "MusicCoCa vocab: " << paths.musiccoca_vocab.string() << "\n";
    out << "Depthformer checkpoint: " << paths.depthformer.string();
    return out.str();
}

void appendEnvPathIfSet (std::ostringstream& out, const char* label, const char* envName)
{
    if (const char* value = std::getenv (envName))
        out << "\n" << label << ": " << value;
}

} // namespace

MagentaRtJuceEngine::MagentaRtJuceEngine() = default;

MagentaRtJuceEngine::~MagentaRtJuceEngine()
{
    stop();
}

void MagentaRtJuceEngine::configure (EngineSettings settings)
{
    stop();

    lastConfigureSettings_ = settings;
    {
        const std::lock_guard lock (loadedWeightPathsMutex_);
        loadedWeightPathsStatus_.clear();
    }

    runtimeSettings_.weightsDirectory = std::move (settings.weightsDirectory);
    runtimeSettings_.depthformerWeightsPath = std::move (settings.depthformerWeightsPath);
    runtimeSettings_.tag = std::move (settings.tag);
    runtimeSettings_.dtype = settings.dtype;
    runtimeSettings_.useMlxfn = settings.useMlxfn;
    runtimeSettings_.chunkLengthSeconds = std::max (0.04f, settings.chunkLengthSeconds);

    {
        const std::lock_guard lock (liveMutex_);
        liveControls_.temperature = settings.temperature;
        liveControls_.topK = settings.topK;
        liveControls_.guidanceWeight = settings.guidanceWeight;
        liveControls_.seed = settings.seed;
        liveControls_.speculative = settings.speculative;
        liveControls_.pipelineEncoder = settings.pipelineEncoder;
        liveControls_.streamingCached = settings.streamingCached;
        liveControls_.encoderRefreshFrames =
            std::max (1, settings.encoderRefreshFrames);
        liveControls_.prebufferChunks =
            std::max (0, settings.prebufferChunks);
        liveControls_.maxQueueChunks =
            std::max (1, settings.maxQueueChunks);
        liveControls_.styleTransitionDelaySeconds =
            std::clamp (settings.styleTransitionDelaySeconds, 0.1, 30.0);
    }

    system_.reset();
    state_.reset();
    refreshPrebufferTargetFrames();
}

const EngineSettings& MagentaRtJuceEngine::settings() const noexcept
{
    return lastConfigureSettings_;
}

RuntimeSettings MagentaRtJuceEngine::runtimeSettings() const
{
    return runtimeSettings_;
}

std::string MagentaRtJuceEngine::resolveConfiguredWeightPathsStatus() const
{
    const auto paths = magenta_realtime_mlx::resolve_inference_bundle (
        runtimeSettings_.weightsDirectory,
        runtimeSettings_.tag,
        runtimeSettings_.depthformerWeightsPath.empty()
            ? std::nullopt
            : std::optional<std::filesystem::path> { runtimeSettings_.depthformerWeightsPath });
    return formatWeightPaths (paths);
}

std::string MagentaRtJuceEngine::loadedWeightPathsStatus() const
{
    const std::lock_guard lock (loadedWeightPathsMutex_);
    return loadedWeightPathsStatus_;
}

LiveControlSnapshot MagentaRtJuceEngine::liveControls() const
{
    const std::lock_guard lock (liveMutex_);
    return liveControls_;
}

void MagentaRtJuceEngine::setLiveControls (LiveControlSnapshot controls)
{
    controls.prebufferChunks = std::max (0, controls.prebufferChunks);
    controls.maxQueueChunks = std::max (1, controls.maxQueueChunks);
    controls.styleTransitionDelaySeconds =
        std::clamp (controls.styleTransitionDelaySeconds, 0.1, 30.0);

    {
        const std::lock_guard lock (liveMutex_);
        liveControls_ = std::move (controls);
        lastConfigureSettings_.temperature = liveControls_.temperature;
        lastConfigureSettings_.topK = liveControls_.topK;
        lastConfigureSettings_.guidanceWeight = liveControls_.guidanceWeight;
        lastConfigureSettings_.seed = liveControls_.seed;
        lastConfigureSettings_.speculative = liveControls_.speculative;
        lastConfigureSettings_.pipelineEncoder = liveControls_.pipelineEncoder;
        lastConfigureSettings_.streamingCached = liveControls_.streamingCached;
        lastConfigureSettings_.encoderRefreshFrames = liveControls_.encoderRefreshFrames;
        lastConfigureSettings_.prebufferChunks = liveControls_.prebufferChunks;
        lastConfigureSettings_.maxQueueChunks = liveControls_.maxQueueChunks;
        lastConfigureSettings_.styleTransitionDelaySeconds =
            liveControls_.styleTransitionDelaySeconds;
    }

    refreshPrebufferTargetFrames();
}

void MagentaRtJuceEngine::setPromptPortfolio (PromptPortfolio portfolio)
{
    {
        const std::lock_guard lock (liveMutex_);
        liveControls_.prompts = std::move (portfolio);
    }
}

void MagentaRtJuceEngine::refreshPrebufferTargetFrames()
{
    std::size_t chunkLen = chunkLengthSamples_.load (std::memory_order_relaxed);
    int preChunks = 0;
    {
        const std::lock_guard lock (liveMutex_);
        preChunks = liveControls_.prebufferChunks;
    }
    prebufferTargetFrames_.store (
        static_cast<std::size_t> (std::max (0, preChunks)) * chunkLen,
        std::memory_order_relaxed);
}

PromptPortfolio MagentaRtJuceEngine::promptPortfolio() const
{
    const std::lock_guard lock (liveMutex_);
    return liveControls_.prompts;
}

PromptSelection MagentaRtJuceEngine::currentPromptSelection() const
{
    const std::lock_guard lock (liveMutex_);
    const auto ordered = liveControls_.prompts.activeSlotsOrdered();
    PromptSelection selection;
    selection.activeCount = ordered.size();
    selection.totalWeight = liveControls_.prompts.totalActiveWeight();

    if (! ordered.empty())
        selection.prompt = ordered[0].text;

    return selection;
}

void MagentaRtJuceEngine::loadRuntime()
{
    applyDepthformerMlxfnEnv (
        runtimeSettings_.weightsDirectory,
        runtimeSettings_.tag,
        runtimeSettings_.dtype,
        runtimeSettings_.useMlxfn);

    std::ostringstream loadedPaths;
    loadedPaths << resolveConfiguredWeightPathsStatus();
    if (runtimeSettings_.useMlxfn)
    {
        appendEnvPathIfSet (loadedPaths, "MLXFN encode", "MRT_DEPTHFORMER_ENCODE_MLXFN");
        appendEnvPathIfSet (
            loadedPaths, "MLXFN temporal directory", "MRT_DEPTHFORMER_TEMPORAL_MLXFN_DIR");
        appendEnvPathIfSet (
            loadedPaths, "MLXFN temporal padded", "MRT_DEPTHFORMER_TEMPORAL_PADDED_MLXFN");
        appendEnvPathIfSet (
            loadedPaths, "MLXFN depth directory", "MRT_DEPTHFORMER_DEPTH_MLXFN_DIR");
        appendEnvPathIfSet (
            loadedPaths, "MLXFN depth padded", "MRT_DEPTHFORMER_DEPTH_PADDED_MLXFN");
    }
    {
        const std::lock_guard lock (loadedWeightPathsMutex_);
        loadedWeightPathsStatus_ = loadedPaths.str();
    }
    std::fprintf (stderr, "Loaded Magenta RT weights:\n%s\n", loadedPaths.str().c_str());

    magenta_realtime_mlx::SystemConfig systemConfig;
    systemConfig.chunk_length = runtimeSettings_.chunkLengthSeconds;
    system_ = std::make_unique<magenta_realtime_mlx::System> (
        runtimeSettings_.weightsDirectory,
        runtimeSettings_.tag,
        mlxDtype(),
        runtimeSettings_.depthformerWeightsPath.empty()
            ? std::nullopt
            : std::optional<std::filesystem::path> { runtimeSettings_.depthformerWeightsPath },
        systemConfig);
    state_ = system_->empty_state();
    chunkLengthSamples_.store (
        static_cast<std::size_t> (system_->config().chunk_length_samples()),
        std::memory_order_relaxed);
    refreshPrebufferTargetFrames();
}

void MagentaRtJuceEngine::unloadRuntime() noexcept
{
    stop();
    state_.reset();
    system_.reset();
    chunkLengthSamples_.store (96000, std::memory_order_relaxed);
    refreshPrebufferTargetFrames();
}

void MagentaRtJuceEngine::reset()
{
    stop();
    if (system_ != nullptr)
        state_ = system_->empty_state();
    else
        state_.reset();
}

bool MagentaRtJuceEngine::isLoaded() const noexcept
{
    return system_ != nullptr;
}

bool MagentaRtJuceEngine::isRunning() const noexcept
{
    return running_.load (std::memory_order_acquire);
}

mlx::core::Dtype MagentaRtJuceEngine::mlxDtype() const
{
    switch (runtimeSettings_.dtype)
    {
        case ComputeDtype::Float32:
            return mlx::core::float32;
        case ComputeDtype::Float16:
            return mlx::core::float16;
        case ComputeDtype::BFloat16:
            return mlx::core::bfloat16;
    }

    return mlx::core::bfloat16;
}

std::string MagentaRtJuceEngine::lastError() const
{
    const std::lock_guard lock (errorMutex_);
    return lastError_;
}

std::size_t MagentaRtJuceEngine::prebufferTargetFrames() const noexcept
{
    return prebufferTargetFrames_.load (std::memory_order_relaxed);
}

void MagentaRtJuceEngine::recordChunkGenerationTiming (double audioSeconds, double wallSeconds)
{
    const double chunkRtf = (wallSeconds > 0.0) ? (audioSeconds / wallSeconds) : 0.0;
    const std::lock_guard lock (timingMutex_);
    rtfAccumulator_.lastChunkRtf = chunkRtf;
    rtfAccumulator_.sumAudioSeconds += audioSeconds;
    rtfAccumulator_.sumWallSeconds += wallSeconds;
    ++rtfAccumulator_.chunks;
}

GenerationRtfStats MagentaRtJuceEngine::generationRtfStats() const
{
    const std::lock_guard lock (timingMutex_);
    GenerationRtfStats out;
    out.lastChunkRtf = rtfAccumulator_.lastChunkRtf;
    out.completedChunks = rtfAccumulator_.chunks;
    out.averageRtf = (rtfAccumulator_.sumWallSeconds > 0.0)
        ? (rtfAccumulator_.sumAudioSeconds / rtfAccumulator_.sumWallSeconds)
        : 0.0;
    return out;
}

void MagentaRtJuceEngine::start (GeneratedAudioQueue& outputQueue)
{
    if (isRunning())
        return;

    setLastError ({});
    {
        const std::lock_guard lock (timingMutex_);
        rtfAccumulator_ = {};
    }
    running_.store (true, std::memory_order_release);
    worker_ = std::thread ([this, &outputQueue] { runGenerationLoop (outputQueue); });
}

void MagentaRtJuceEngine::stop()
{
    running_.store (false, std::memory_order_release);
    if (worker_.joinable())
        worker_.join();
}

magenta_realtime_mlx::GenerateChunkOptions MagentaRtJuceEngine::makeGenerateOptions (
    std::uint64_t chunkIndex) const
{
    return makeGenerateOptions (chunkIndex, liveControls());
}

magenta_realtime_mlx::GenerateChunkOptions MagentaRtJuceEngine::makeGenerateOptions (
    std::uint64_t chunkIndex, const LiveControlSnapshot& controls) const
{
    magenta_realtime_mlx::GenerateChunkOptions options;
    options.temperature = controls.temperature;
    options.top_k = controls.topK;
    options.guidance_weight = controls.guidanceWeight;
    options.speculative = controls.speculative;
    options.pipeline_encoder = controls.pipelineEncoder;
    options.streaming_cached = controls.streamingCached;
    options.encoder_refresh_frames = std::max (1, controls.encoderRefreshFrames);

    if (controls.seed.has_value())
        options.seed = *controls.seed + chunkIndex;

    return options;
}

std::vector<magenta_realtime_mlx::WeightedTextPrompt> MagentaRtJuceEngine::toWeightedMlx (
    const PromptPortfolio& portfolio)
{
    std::vector<magenta_realtime_mlx::WeightedTextPrompt> out;
    for (const auto& row : portfolio.activeSlotsOrdered())
    {
        magenta_realtime_mlx::WeightedTextPrompt p;
        p.text = row.text;
        p.weight = row.weight;
        out.push_back (std::move (p));
    }
    return out;
}

void MagentaRtJuceEngine::runGenerationLoop (GeneratedAudioQueue& outputQueue)
{
    try
    {
        if (system_ == nullptr)
            loadRuntime();

        std::string styleCacheSignature;
        std::optional<mlx::core::array> styleCacheTokens;

        while (running_.load (std::memory_order_acquire))
        {
            LiveControlSnapshot controls;
            {
                const std::lock_guard lock (liveMutex_);
                controls = liveControls_;
            }

            if (! state_.has_value())
                state_ = system_->empty_state();

            const auto maxQueuedFrames =
                static_cast<std::size_t> (controls.maxQueueChunks)
                * static_cast<std::size_t> (system_->config().chunk_length_samples());
            if (outputQueue.queuedFrames() >= maxQueuedFrames)
            {
                std::this_thread::sleep_for (std::chrono::milliseconds (50));
                continue;
            }

            const std::string sig = controls.prompts.signature();
            std::optional<mlx::core::array> styleTokens;

            if (! sig.empty())
            {
                if (styleCacheSignature != sig || ! styleCacheTokens.has_value())
                {
                    const auto weighted = toWeightedMlx (controls.prompts);
                    styleCacheTokens =
                        system_->embed_style_weighted_text (weighted);
                    styleCacheSignature = sig;
                }
                styleTokens = *styleCacheTokens;
            }
            else
            {
                styleCacheSignature.clear();
                styleCacheTokens.reset();
                styleTokens = std::nullopt;
            }

            const auto t0 = std::chrono::steady_clock::now();
            auto result = system_->generate_chunk (
                *state_, styleTokens,
                makeGenerateOptions (static_cast<std::uint64_t> (state_->chunk_index),
                    controls));
            const auto t1 = std::chrono::steady_clock::now();
            const double wallSeconds =
                std::chrono::duration<double> (t1 - t0).count();
            recordChunkGenerationTiming (result.waveform.duration(), wallSeconds);

            std::vector<float> samples (result.waveform.samples.size());
            std::memcpy (samples.data(), result.waveform.samples.data<float>(),
                samples.size() * sizeof (float));

            while (running_.load (std::memory_order_acquire)
                   && ! outputQueue.pushInterleaved (
                       samples.data(),
                       static_cast<std::size_t> (result.waveform.num_samples()),
                       result.waveform.num_channels()))
            {
                std::this_thread::sleep_for (std::chrono::milliseconds (20));
            }
        }
    }
    catch (const std::exception& ex)
    {
        setLastError (ex.what());
    }
    catch (...)
    {
        setLastError ("unknown generation error");
    }

    running_.store (false, std::memory_order_release);
}

void MagentaRtJuceEngine::setLastError (std::string message)
{
    const std::lock_guard lock (errorMutex_);
    lastError_ = std::move (message);
}

} // namespace mrt::plugin
