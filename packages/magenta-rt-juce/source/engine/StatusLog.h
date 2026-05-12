#pragma once

#include <optional>
#include <string>

namespace mrt::plugin
{

enum class RunningStatus
{
    Prebuffering,
    Playing
};

class StatusLog
{
public:
    void setMessage (std::string message);
    [[nodiscard]] bool updateRunningStatus (RunningStatus status, std::string loadedWeightsBlock);
    [[nodiscard]] const std::string& visibleText() const noexcept;

private:
    std::optional<RunningStatus> runningStatus_;
    std::string loadedWeightsBlock_;
    std::string visibleText_;
};

} // namespace mrt::plugin
