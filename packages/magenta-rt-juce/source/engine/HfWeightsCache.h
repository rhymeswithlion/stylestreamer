#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace mrt::plugin
{

struct HfWeightsCacheConfig
{
    std::filesystem::path cacheRoot {};
    std::string repoId { "rhymeswithlion/magenta-realtime-mlx-cpp" };
    std::string repoType { "dataset" };
    std::string revision { "main" };
    std::string depthformerTag { "base" };
    std::string mlxfnDtype { "bf16" };
    bool requireMlxfn { true };
};

[[nodiscard]] std::filesystem::path hfHubCacheRoot();
[[nodiscard]] std::filesystem::path hfRepoCacheDirectory (
    const std::filesystem::path& cacheRoot,
    const std::string& repoId,
    const std::string& repoType);
[[nodiscard]] std::vector<std::filesystem::path> requiredHfWeightFiles (
    const std::string& depthformerTag);
[[nodiscard]] bool isCompleteHfWeightsSnapshot (
    const std::filesystem::path& snapshot,
    const HfWeightsCacheConfig& config);
[[nodiscard]] std::optional<std::filesystem::path> findCompleteHfWeightsSnapshot (
    HfWeightsCacheConfig config = {});
[[nodiscard]] std::filesystem::path expectedHfWeightsCacheDirectory (
    HfWeightsCacheConfig config = {});

} // namespace mrt::plugin
