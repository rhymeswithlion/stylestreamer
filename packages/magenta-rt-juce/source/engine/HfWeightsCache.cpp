#include "engine/HfWeightsCache.h"

#include <cstdlib>
#include <fstream>
#include <string_view>

namespace mrt::plugin
{
namespace
{

[[nodiscard]] bool envIsSet (const char* value) noexcept
{
    return value != nullptr && value[0] != '\0';
}

[[nodiscard]] std::filesystem::path homeDirectory()
{
    if (const char* home = std::getenv ("HOME"); envIsSet (home))
        return std::filesystem::path (home);
    return std::filesystem::current_path();
}

[[nodiscard]] std::string trim (std::string text)
{
    const auto first = text.find_first_not_of (" \t\r\n");
    if (first == std::string::npos)
        return {};
    const auto last = text.find_last_not_of (" \t\r\n");
    return text.substr (first, last - first + 1);
}

[[nodiscard]] std::optional<std::string> readRef (const std::filesystem::path& path)
{
    std::ifstream in (path);
    if (! in)
        return std::nullopt;

    std::string text;
    std::getline (in, text, '\0');
    text = trim (text);
    if (text.empty())
        return std::nullopt;
    return text;
}

[[nodiscard]] std::string repoDirectoryPrefix (std::string_view repoType)
{
    if (repoType == "model")
        return "models--";
    if (repoType == "space")
        return "spaces--";
    return "datasets--";
}

[[nodiscard]] std::string repoDirectoryName (
    const std::string& repoId,
    const std::string& repoType)
{
    std::string encoded = repoDirectoryPrefix (repoType);
    encoded.reserve (encoded.size() + repoId.size() + 2);
    for (char c : repoId)
    {
        if (c == '/')
            encoded += "--";
        else
            encoded += c;
    }
    return encoded;
}

[[nodiscard]] bool hasFile (const std::filesystem::path& path)
{
    return std::filesystem::is_regular_file (path);
}

[[nodiscard]] bool hasMlxfnBundle (
    const std::filesystem::path& snapshot,
    const HfWeightsCacheConfig& config)
{
    const auto mlxfnDir = snapshot / "mlxfn";
    const std::string suffix =
        "_" + config.depthformerTag + "_" + config.mlxfnDtype;

    const bool haveEncode = hasFile (mlxfnDir / ("encode" + suffix + ".mlxfn"));
    const bool haveDepth =
        hasFile (mlxfnDir / ("depth_step" + suffix + "_cl01.mlxfn"))
        || hasFile (mlxfnDir / ("depth_step_padded" + suffix + ".mlxfn"));
    const bool haveTemporal =
        hasFile (mlxfnDir / ("temporal_step" + suffix + "_cl01.mlxfn"))
        || hasFile (mlxfnDir / ("temporal_step_padded" + suffix + ".mlxfn"));

    return haveEncode && haveDepth && haveTemporal;
}

} // namespace

std::filesystem::path hfHubCacheRoot()
{
    if (const char* cache = std::getenv ("HF_HUB_CACHE"); envIsSet (cache))
        return std::filesystem::path (cache);
    if (const char* home = std::getenv ("HF_HOME"); envIsSet (home))
        return std::filesystem::path (home) / "hub";
    return homeDirectory() / ".cache" / "huggingface" / "hub";
}

std::filesystem::path hfRepoCacheDirectory (
    const std::filesystem::path& cacheRoot,
    const std::string& repoId,
    const std::string& repoType)
{
    return cacheRoot / repoDirectoryName (repoId, repoType);
}

std::vector<std::filesystem::path> requiredHfWeightFiles (
    const std::string& depthformerTag)
{
    return {
        "spectrostream_encoder.safetensors",
        "spectrostream_decoder.safetensors",
        "spectrostream_codebooks.safetensors",
        "musiccoca_encoder.safetensors",
        "musiccoca_codebooks.safetensors",
        "musiccoca_vocab.model",
        std::filesystem::path ("depthformer") / ("depthformer_" + depthformerTag + ".safetensors"),
    };
}

bool isCompleteHfWeightsSnapshot (
    const std::filesystem::path& snapshot,
    const HfWeightsCacheConfig& config)
{
    for (const auto& relative : requiredHfWeightFiles (config.depthformerTag))
        if (! hasFile (snapshot / relative))
            return false;

    return ! config.requireMlxfn || hasMlxfnBundle (snapshot, config);
}

std::optional<std::filesystem::path> findCompleteHfWeightsSnapshot (
    HfWeightsCacheConfig config)
{
    if (config.cacheRoot.empty())
        config.cacheRoot = hfHubCacheRoot();

    const auto repoDir =
        hfRepoCacheDirectory (config.cacheRoot, config.repoId, config.repoType);
    const auto snapshotsDir = repoDir / "snapshots";

    std::vector<std::string> revisions;
    if (! config.revision.empty())
    {
        if (const auto ref = readRef (repoDir / "refs" / config.revision))
            revisions.push_back (*ref);
        revisions.push_back (config.revision);
    }

    for (const auto& revision : revisions)
    {
        const auto snapshot = snapshotsDir / revision;
        if (isCompleteHfWeightsSnapshot (snapshot, config))
            return snapshot;
    }

    if (std::filesystem::is_directory (snapshotsDir))
    {
        for (const auto& entry : std::filesystem::directory_iterator (snapshotsDir))
            if (entry.is_directory() && isCompleteHfWeightsSnapshot (entry.path(), config))
                return entry.path();
    }

    return std::nullopt;
}

std::filesystem::path expectedHfWeightsCacheDirectory (
    HfWeightsCacheConfig config)
{
    if (config.cacheRoot.empty())
        config.cacheRoot = hfHubCacheRoot();
    return hfRepoCacheDirectory (config.cacheRoot, config.repoId, config.repoType);
}

} // namespace mrt::plugin
