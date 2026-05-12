#pragma once

#include "engine/GeneratedAudioQueue.h"
#include "engine/PromptPortfolio.h"
#include "magenta_realtime_mlx/system.h"

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

namespace mrt::plugin
{

enum class ComputeDtype
{
    Float32,
    Float16,
    BFloat16
};

struct RuntimeSettings
{
    std::filesystem::path weightsDirectory;
    std::filesystem::path depthformerWeightsPath;
    std::string tag = "base";
    ComputeDtype dtype = ComputeDtype::BFloat16;
    bool useMlxfn = true;
    float chunkLengthSeconds = 0.4f;
};

struct LiveControlSnapshot
{
    PromptPortfolio prompts { 4 };
    float temperature = 1.1f;
    int topK = 40;
    float guidanceWeight = 5.0f;
    std::optional<std::uint64_t> seed;
    bool speculative = false;
    bool pipelineEncoder = true;
    bool streamingCached = false;
    int encoderRefreshFrames = 5;
    int prebufferChunks = 2;
    int maxQueueChunks = 3;
    double styleTransitionDelaySeconds = 4.0;
};

struct EngineSettings
{
    std::filesystem::path weightsDirectory;
    std::filesystem::path depthformerWeightsPath;
    std::string tag = "base";
    ComputeDtype dtype = ComputeDtype::BFloat16;
    float temperature = 1.1f;
    int topK = 40;
    float guidanceWeight = 5.0f;
    std::optional<std::uint64_t> seed;
    bool speculative = false;
    bool pipelineEncoder = true;
    bool useMlxfn = true;
    float chunkLengthSeconds = 0.4f;
    bool streamingCached = false;
    int encoderRefreshFrames = 5;
    int prebufferChunks = 2;
    int maxQueueChunks = 3;
    double styleTransitionDelaySeconds = 4.0;
};

struct PromptSelection
{
    std::optional<std::string> prompt;
    std::size_t activeCount = 0;
    float totalWeight = 0.0f;
};

/** Rolling RTF from completed ``generate_chunk`` calls (audio seconds / wall seconds). */
struct GenerationRtfStats
{
    double lastChunkRtf = 0.0;
    double averageRtf = 0.0;
    std::uint64_t completedChunks = 0;
};

class MagentaRtJuceEngine
{
public:
    MagentaRtJuceEngine();
    ~MagentaRtJuceEngine();

    void configure (EngineSettings settings);
    [[nodiscard]] const EngineSettings& settings() const noexcept;
    [[nodiscard]] RuntimeSettings runtimeSettings() const;
    [[nodiscard]] LiveControlSnapshot liveControls() const;
    [[nodiscard]] std::string resolveConfiguredWeightPathsStatus() const;
    [[nodiscard]] std::string loadedWeightPathsStatus() const;

    void setLiveControls (LiveControlSnapshot controls);
    void setPromptPortfolio (PromptPortfolio portfolio);

    [[nodiscard]] PromptPortfolio promptPortfolio() const;
    [[nodiscard]] PromptSelection currentPromptSelection() const;

    void loadRuntime();
    void unloadRuntime() noexcept;
    void reset();
    [[nodiscard]] bool isLoaded() const noexcept;
    [[nodiscard]] bool isRunning() const noexcept;
    [[nodiscard]] mlx::core::Dtype mlxDtype() const;
    [[nodiscard]] std::string lastError() const;

    [[nodiscard]] std::size_t prebufferTargetFrames() const noexcept;

    /** RTF for the last completed chunk and the average since the current run's Start. */
    [[nodiscard]] GenerationRtfStats generationRtfStats() const;

    void start (GeneratedAudioQueue& outputQueue);
    void stop();

    [[nodiscard]] magenta_realtime_mlx::GenerateChunkOptions makeGenerateOptions (
        std::uint64_t chunkIndex) const;
    [[nodiscard]] magenta_realtime_mlx::GenerateChunkOptions makeGenerateOptions (
        std::uint64_t chunkIndex, const LiveControlSnapshot& controls) const;

private:
    void runGenerationLoop (GeneratedAudioQueue& outputQueue);
    void setLastError (std::string message);
    void refreshPrebufferTargetFrames();
    void recordChunkGenerationTiming (double audioSeconds, double wallSeconds);
    [[nodiscard]] static std::vector<magenta_realtime_mlx::WeightedTextPrompt> toWeightedMlx (
        const PromptPortfolio& portfolio);

    RuntimeSettings runtimeSettings_;
    LiveControlSnapshot liveControls_;
    mutable std::mutex liveMutex_;
    EngineSettings lastConfigureSettings_;

    std::unique_ptr<magenta_realtime_mlx::System> system_;
    std::optional<magenta_realtime_mlx::SystemState> state_;
    std::thread worker_;
    std::atomic<bool> running_ { false };
    mutable std::mutex errorMutex_;
    std::string lastError_;
    mutable std::mutex loadedWeightPathsMutex_;
    std::string loadedWeightPathsStatus_;

    std::atomic<std::size_t> chunkLengthSamples_ { 96000 };
    std::atomic<std::size_t> prebufferTargetFrames_ { 0 };

    struct RtfAccumulator
    {
        double lastChunkRtf = 0.0;
        double sumAudioSeconds = 0.0;
        double sumWallSeconds = 0.0;
        std::uint64_t chunks = 0;
    };

    mutable std::mutex timingMutex_;
    RtfAccumulator rtfAccumulator_;
};

} // namespace mrt::plugin
