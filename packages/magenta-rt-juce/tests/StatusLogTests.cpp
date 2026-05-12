#include <catch2/catch_test_macros.hpp>

#include "engine/StatusLog.h"

TEST_CASE ("StatusLog suppresses repeated running status rewrites", "[status]")
{
    mrt::plugin::StatusLog log;

    CHECK (log.updateRunningStatus (mrt::plugin::RunningStatus::Prebuffering, "Loaded weights:\nA"));
    CHECK_FALSE (
        log.updateRunningStatus (mrt::plugin::RunningStatus::Prebuffering, "Loaded weights:\nA"));
    CHECK (log.visibleText().find ("Loaded weights:\nA") != std::string::npos);

    CHECK (log.updateRunningStatus (mrt::plugin::RunningStatus::Playing, "Loaded weights:\nA"));
    CHECK_FALSE (log.updateRunningStatus (mrt::plugin::RunningStatus::Playing, "Loaded weights:\nA"));
    CHECK (log.visibleText().find ("Playing") != std::string::npos);
}

TEST_CASE ("StatusLog rewrites when loaded weights change", "[status]")
{
    mrt::plugin::StatusLog log;

    CHECK (log.updateRunningStatus (mrt::plugin::RunningStatus::Playing, "Loaded weights:\nA"));
    CHECK (log.updateRunningStatus (mrt::plugin::RunningStatus::Playing, "Loaded weights:\nB"));
    CHECK (log.visibleText().find ("Loaded weights:\nB") != std::string::npos);
}
