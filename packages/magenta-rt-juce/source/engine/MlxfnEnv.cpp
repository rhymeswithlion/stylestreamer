#include "engine/MlxfnEnv.h"

#include "engine/MagentaRtJuceEngine.h"

#include <cstdlib>
#include <sstream>
#include <stdexcept>
#include <string>

namespace mrt::plugin
{
namespace
{

constexpr const char* kEnvEncode = "MRT_DEPTHFORMER_ENCODE_MLXFN";
constexpr const char* kEnvTemporalDir = "MRT_DEPTHFORMER_TEMPORAL_MLXFN_DIR";
constexpr const char* kEnvTemporalPadded = "MRT_DEPTHFORMER_TEMPORAL_PADDED_MLXFN";
constexpr const char* kEnvDepthDir = "MRT_DEPTHFORMER_DEPTH_MLXFN_DIR";
constexpr const char* kEnvDepthPadded = "MRT_DEPTHFORMER_DEPTH_PADDED_MLXFN";

const char* mlxDtypeLabel (ComputeDtype dtype) noexcept
{
    switch (dtype)
    {
        case ComputeDtype::Float32:
            return "fp32";
        case ComputeDtype::Float16:
            return "fp16";
        case ComputeDtype::BFloat16:
            return "bf16";
    }

    return "bf16";
}

} // namespace

void clearDepthformerMlxfnEnv() noexcept
{
    for (const char* name : { kEnvEncode,
             kEnvTemporalDir,
             kEnvTemporalPadded,
             kEnvDepthDir,
             kEnvDepthPadded })
        ::unsetenv (name);
}

void applyDepthformerMlxfnEnv (
    const std::filesystem::path& weights_dir,
    std::string_view tag,
    ComputeDtype dtype,
    bool use_mlxfn)
{
    clearDepthformerMlxfnEnv();

    if (! use_mlxfn)
        return;

    const std::filesystem::path mlxfn_dir = weights_dir / "mlxfn";
    if (! std::filesystem::is_directory (mlxfn_dir))
    {
        throw std::runtime_error (
            std::string ("mlxfn: directory not found (required when MLXFN is enabled): ")
            + mlxfn_dir.string());
    }

    const std::string dtype_part = mlxDtypeLabel (dtype);
    const std::string suffix =
        std::string ("_") + std::string (tag) + "_" + dtype_part;

    const std::filesystem::path encode_path =
        mlxfn_dir / ("encode" + suffix + ".mlxfn");
    const bool have_encode = std::filesystem::is_regular_file (encode_path);

    const std::filesystem::path padded_temporal_path =
        mlxfn_dir / ("temporal_step_padded" + suffix + ".mlxfn");
    const std::filesystem::path padded_depth_path =
        mlxfn_dir / ("depth_step_padded" + suffix + ".mlxfn");
    const bool have_padded_temporal =
        std::filesystem::is_regular_file (padded_temporal_path);
    const bool have_padded_depth =
        std::filesystem::is_regular_file (padded_depth_path);

    const bool have_depth_per_cl = std::filesystem::is_regular_file (
        mlxfn_dir / ("depth_step" + suffix + "_cl01.mlxfn"));
    const bool have_temporal_per_cl = std::filesystem::is_regular_file (
        mlxfn_dir / ("temporal_step" + suffix + "_cl01.mlxfn"));

    const bool depth_active = have_depth_per_cl || have_padded_depth;
    const bool temporal_active = have_temporal_per_cl || have_padded_temporal;

    const int loaded_count = (have_encode ? 1 : 0) + (depth_active ? 1 : 0)
        + (temporal_active ? 1 : 0);

    if (loaded_count != 3)
    {
        std::ostringstream msg;
        msg << "mlxfn: incomplete bundle under " << mlxfn_dir.string()
            << " for tag=" << tag << " dtype=" << dtype_part
            << " (suffix " << suffix << "): require encode, depth, and temporal; missing ";
        if (! have_encode)
            msg << "encode ";
        if (! depth_active)
            msg << "depth ";
        if (! temporal_active)
            msg << "temporal ";
        throw std::runtime_error (msg.str());
    }

    ::setenv (kEnvEncode, encode_path.string().c_str(), /*overwrite=*/1);

    // Per-cl beats padded for depth/temporal when both exist (matches CLI).
    if (have_temporal_per_cl)
        ::setenv (
            kEnvTemporalDir, mlxfn_dir.string().c_str(), /*overwrite=*/1);
    else if (have_padded_temporal)
        ::setenv (
            kEnvTemporalPadded,
            padded_temporal_path.string().c_str(),
            /*overwrite=*/1);

    if (have_depth_per_cl)
        ::setenv (
            kEnvDepthDir, mlxfn_dir.string().c_str(), /*overwrite=*/1);
    else if (have_padded_depth)
        ::setenv (
            kEnvDepthPadded,
            padded_depth_path.string().c_str(),
            /*overwrite=*/1);
}

} // namespace mrt::plugin
