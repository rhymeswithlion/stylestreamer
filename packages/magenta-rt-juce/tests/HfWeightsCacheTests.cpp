#include "engine/HfWeightsCache.h"

#include <catch2/catch_test_macros.hpp>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>

namespace
{

class ScopedEnv
{
public:
    explicit ScopedEnv (const char* name_in)
        : name (name_in)
    {
        if (const char* current = std::getenv (name.c_str()))
            previous = std::string (current);
    }

    ~ScopedEnv()
    {
        if (previous.has_value())
            ::setenv (name.c_str(), previous->c_str(), 1);
        else
            ::unsetenv (name.c_str());
    }

    void set (const std::filesystem::path& value) const
    {
        ::setenv (name.c_str(), value.string().c_str(), 1);
    }

    void unset() const
    {
        ::unsetenv (name.c_str());
    }

private:
    std::string name;
    std::optional<std::string> previous;
};

[[nodiscard]] std::filesystem::path tempDir (std::string_view name)
{
    const auto root = std::filesystem::temp_directory_path()
        / ("stylestreamer-hf-cache-tests-" + std::string (name));
    std::filesystem::remove_all (root);
    std::filesystem::create_directories (root);
    return root;
}

void touch (const std::filesystem::path& path)
{
    std::filesystem::create_directories (path.parent_path());
    std::ofstream out (path);
    out << "x";
}

void writeText (const std::filesystem::path& path, std::string_view text)
{
    std::filesystem::create_directories (path.parent_path());
    std::ofstream out (path);
    out << text;
}

void addRequiredBundleFiles (const std::filesystem::path& snapshot)
{
    for (const auto& relative : mrt::plugin::requiredHfWeightFiles ("base"))
        touch (snapshot / relative);
}

} // namespace

TEST_CASE ("HF cache root follows Hugging Face environment precedence", "[HfWeightsCache]")
{
    ScopedEnv hfHubCache ("HF_HUB_CACHE");
    ScopedEnv hfHome ("HF_HOME");
    const auto root = tempDir ("env");
    hfHubCache.set (root / "explicit-hub");
    hfHome.set (root / "home");

    CHECK (mrt::plugin::hfHubCacheRoot() == root / "explicit-hub");

    hfHubCache.unset();
    CHECK (mrt::plugin::hfHubCacheRoot() == root / "home" / "hub");
}

TEST_CASE ("HF cache resolver finds a complete snapshot through refs", "[HfWeightsCache]")
{
    const auto root = tempDir ("snapshot");
    const std::string sha = "0123456789abcdef0123456789abcdef01234567";
    const auto repoDir = root / "datasets--rhymeswithlion--magenta-realtime-mlx-cpp";
    const auto snapshot = repoDir / "snapshots" / sha;
    writeText (repoDir / "refs" / "main", sha);
    addRequiredBundleFiles (snapshot);

    mrt::plugin::HfWeightsCacheConfig config;
    config.cacheRoot = root;
    config.revision = "main";
    config.depthformerTag = "base";
    config.requireMlxfn = false;

    const auto resolved = mrt::plugin::findCompleteHfWeightsSnapshot (config);

    REQUIRE (resolved.has_value());
    CHECK (*resolved == snapshot);
}

TEST_CASE ("HF cache resolver requires MLXFN bundle when requested", "[HfWeightsCache]")
{
    const auto root = tempDir ("mlxfn");
    const std::string sha = "fedcba9876543210fedcba9876543210fedcba98";
    const auto repoDir = root / "datasets--rhymeswithlion--magenta-realtime-mlx-cpp";
    const auto snapshot = repoDir / "snapshots" / sha;
    writeText (repoDir / "refs" / "main", sha);
    addRequiredBundleFiles (snapshot);

    mrt::plugin::HfWeightsCacheConfig config;
    config.cacheRoot = root;
    config.revision = "main";
    config.depthformerTag = "base";
    config.mlxfnDtype = "bf16";
    config.requireMlxfn = true;

    CHECK_FALSE (mrt::plugin::findCompleteHfWeightsSnapshot (config).has_value());

    touch (snapshot / "mlxfn" / "encode_base_bf16.mlxfn");
    touch (snapshot / "mlxfn" / "depth_step_padded_base_bf16.mlxfn");
    touch (snapshot / "mlxfn" / "temporal_step_padded_base_bf16.mlxfn");

    const auto resolved = mrt::plugin::findCompleteHfWeightsSnapshot (config);

    REQUIRE (resolved.has_value());
    CHECK (*resolved == snapshot);
}
