#pragma once

#include <filesystem>
#include <string_view>

namespace mrt::plugin
{

enum class ComputeDtype;

/** Remove Depthformer ``.mlxfn`` env vars so stale shell/session values cannot
    override the Standalone / plugin configuration. */
void clearDepthformerMlxfnEnv() noexcept;

/** With ``use_mlxfn`` true, require ``<weights>/mlxfn`` plus a full encode,
    depth (per-cl or padded), and temporal (per-cl or padded) trio for ``tag``
    and ``dtype``, then set ``MRT_DEPTHFORMER_*`` before ``System``
    construction. With ``use_mlxfn`` false, only clears those variables. */
void applyDepthformerMlxfnEnv (
    const std::filesystem::path& weights_dir,
    std::string_view tag,
    ComputeDtype dtype,
    bool use_mlxfn);

} // namespace mrt::plugin
