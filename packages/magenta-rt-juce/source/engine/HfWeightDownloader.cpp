#include "engine/HfWeightDownloader.h"

#include <cstdlib>

namespace mrt::plugin
{
namespace
{

[[nodiscard]] juce::String basePythonExecutable()
{
    if (const char* env = std::getenv ("MRT_JUCE_PYTHON"))
        if (env[0] != '\0')
            return juce::String (env);

    return "python3";
}

[[nodiscard]] juce::String shellQuote (const juce::String& text)
{
    return "\"" + text.replace ("\\", "\\\\").replace ("\"", "\\\"") + "\"";
}

[[nodiscard]] juce::File venvPython (const juce::File& venvDir)
{
    return venvDir.getChildFile ("bin/python3");
}

} // namespace

juce::File defaultWeightHelperVenvDir()
{
    return juce::File::getSpecialLocation (juce::File::tempDirectory)
        .getChildFile ("stylestreamer-hf-weight-helper-venv");
}

juce::StringArray buildWeightDownloadProcessArgs (
    const juce::File& helperScript,
    const juce::File& venvDir,
    bool includeMlxfn)
{
    const auto helperDir = helperScript.getParentDirectory();
    const auto requirements = helperDir.getChildFile ("requirements.txt");
    const auto python = venvPython (venvDir);

    juce::String command;
    command << shellQuote (basePythonExecutable())
            << " -m venv " << shellQuote (venvDir.getFullPathName())
            << " && " << shellQuote (python.getFullPathName())
            << " -m pip install --upgrade -r " << shellQuote (requirements.getFullPathName())
            << " && " << shellQuote (python.getFullPathName())
            << " " << shellQuote (helperScript.getFullPathName())
            << " --revision main --print-snapshot-dir";

    if (! includeMlxfn)
        command << " --no-mlxfn";

    return { "/bin/sh", "-lc", command };
}

} // namespace mrt::plugin
