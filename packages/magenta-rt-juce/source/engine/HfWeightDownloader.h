#pragma once

#include <juce_core/juce_core.h>

namespace mrt::plugin
{

[[nodiscard]] juce::File defaultWeightHelperVenvDir();
[[nodiscard]] juce::StringArray buildWeightDownloadProcessArgs (
    const juce::File& helperScript,
    const juce::File& venvDir,
    bool includeMlxfn);

} // namespace mrt::plugin
