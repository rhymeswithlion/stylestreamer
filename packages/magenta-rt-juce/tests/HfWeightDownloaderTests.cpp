#include "engine/HfWeightDownloader.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE ("HF weight downloader bootstraps an isolated helper venv", "[HfWeightDownloader]")
{
    const juce::File helper ("/tmp/Style Streamer Helper/download_hf_weights.py");
    const juce::File venv ("/tmp/Style Streamer Helper/.venv");

    const auto args = mrt::plugin::buildWeightDownloadProcessArgs (
        helper,
        venv,
        true);

    REQUIRE (args.size() == 3);
    CHECK (args[0] == "/bin/sh");
    CHECK (args[1] == "-lc");

    const auto command = args[2];
    CHECK (command.contains ("\"python3\" -m venv"));
    CHECK (command.contains ("\"/tmp/Style Streamer Helper/.venv/bin/python3\" -m pip install"));
    CHECK (command.contains ("\"/tmp/Style Streamer Helper/requirements.txt\""));
    CHECK (command.contains ("\"/tmp/Style Streamer Helper/.venv/bin/python3\" \"/tmp/Style Streamer Helper/download_hf_weights.py\""));
    CHECK (command.contains ("--print-snapshot-dir"));
    CHECK_FALSE (command.contains ("python3 -m pip install"));
}

TEST_CASE ("HF weight downloader can skip MLXFN bundles", "[HfWeightDownloader]")
{
    const juce::File helper ("/tmp/helper/download_hf_weights.py");
    const juce::File venv ("/tmp/helper/.venv");

    const auto args = mrt::plugin::buildWeightDownloadProcessArgs (
        helper,
        venv,
        false);

    REQUIRE (args.size() == 3);
    CHECK (args[2].contains ("--no-mlxfn"));
}
