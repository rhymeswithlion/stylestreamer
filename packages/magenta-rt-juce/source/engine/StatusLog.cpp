#include "engine/StatusLog.h"

#include <utility>

namespace mrt::plugin
{
namespace
{

[[nodiscard]] const char* runningStatusText (RunningStatus status) noexcept
{
    switch (status)
    {
        case RunningStatus::Prebuffering:
            return "Prebuffering (no audio until full)";
        case RunningStatus::Playing:
            return "Playing";
    }

    return "Playing";
}

} // namespace

void StatusLog::setMessage (std::string message)
{
    runningStatus_.reset();
    loadedWeightsBlock_.clear();
    visibleText_ = std::move (message);
}

bool StatusLog::updateRunningStatus (RunningStatus status, std::string loadedWeightsBlock)
{
    if (runningStatus_.has_value() && *runningStatus_ == status
        && loadedWeightsBlock_ == loadedWeightsBlock)
        return false;

    runningStatus_ = status;
    loadedWeightsBlock_ = std::move (loadedWeightsBlock);
    visibleText_ = runningStatusText (status);
    if (! loadedWeightsBlock_.empty())
        visibleText_ += "\n\n" + loadedWeightsBlock_;
    return true;
}

const std::string& StatusLog::visibleText() const noexcept
{
    return visibleText_;
}

} // namespace mrt::plugin
