#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <cstring>

#include "engine/PromptStateClipboard.h"

TEST_CASE ("Prompt state clipboard encodes versioned base64 JSON", "[prompt-state]")
{
    mrt::plugin::PromptStateClipboardPayload payload;
    payload.prompts[0] = { "deep house", 75.0 };
    payload.prompts[1] = { "driving bass", 25.0 };
    payload.prompts[2] = { "", 0.0 };
    payload.prompts[3] = { "shuffled hats", 10.0 };
    payload.settings.seed = "123";
    payload.settings.temperature = 0.85;
    payload.settings.topK = 64;
    payload.settings.guidanceWeight = 4.5;
    payload.settings.prebufferChunks = 3;
    payload.settings.maxQueueChunks = 6;

    const auto encoded = mrt::plugin::encodePromptStateClipboardPayload (payload);
    CHECK (encoded.startsWith ("eyJ"));
    const auto decoded = mrt::plugin::decodePromptStateClipboardPayload (encoded);

    REQUIRE (decoded.ok);
    CHECK (decoded.payload.version == 3);
    CHECK (decoded.payload.prompts[0].text == "deep house");
    CHECK (decoded.payload.prompts[0].weightPercent == Catch::Approx (75.0));
    CHECK (decoded.payload.prompts[1].text == "driving bass");
    CHECK (decoded.payload.prompts[1].weightPercent == Catch::Approx (25.0));
    CHECK (decoded.payload.prompts[2].text.isEmpty());
    CHECK (decoded.payload.prompts[2].weightPercent == Catch::Approx (0.0));
    CHECK (decoded.payload.prompts[3].text == "shuffled hats");
    CHECK (decoded.payload.prompts[3].weightPercent == Catch::Approx (10.0));
    CHECK (decoded.payload.settings.seed == "123");
    CHECK (decoded.payload.settings.temperature == Catch::Approx (0.85));
    CHECK (decoded.payload.settings.topK == 64);
    CHECK (decoded.payload.settings.guidanceWeight == Catch::Approx (4.5));
    CHECK (decoded.payload.settings.prebufferChunks == 3);
    CHECK (decoded.payload.settings.maxQueueChunks == 6);
}

TEST_CASE ("Prompt state clipboard accepts quoted base64 text", "[prompt-state]")
{
    mrt::plugin::PromptStateClipboardPayload payload;
    payload.prompts[0] = { "brostep festival drop", 88.0 };

    const auto encoded = mrt::plugin::encodePromptStateClipboardPayload (payload);
    const auto decoded = mrt::plugin::decodePromptStateClipboardPayload (
        "\"" + encoded + "\"");

    REQUIRE (decoded.ok);
    CHECK (decoded.payload.prompts[0].text == "brostep festival drop");
    CHECK (decoded.payload.prompts[0].weightPercent == Catch::Approx (88.0));
}

TEST_CASE ("Prompt state clipboard rejects legacy base64 JSON", "[prompt-state]")
{
    const auto decoded = mrt::plugin::decodePromptStateClipboardPayload (
        "eyJ2ZXJzaW9uIjoxLCJwcm9tcHRzIjpbeyJ0ZXh0IjoiYnJvc3RlcCBmZXN0aXZhbCBkcm9wIiwid2VpZ2h0UGVyY2VudCI6ODh9LHsidGV4dCI6ImRydW0gYW5kIGJhc3Mgcm9sbGVyIiwid2VpZ2h0UGVyY2VudCI6ODN9LHsidGV4dCI6ImV1cGhvcmljIGZlc3RpdmFsIGFudGhlbSIsIndlaWdodFBlcmNlbnQiOjc4fSx7InRleHQiOiJzb3VsZnVsIGdvc3BlbCBjaG9pciIsIndlaWdodFBlcmNlbnQiOjU5fV0sInNldHRpbmdzIjp7InNlZWQiOiIiLCJ0ZW1wZXJhdHVyZSI6MS4xOSwidG9wSyI6NDQsImd1aWRhbmNlV2VpZ2h0Ijo1LjAsInByZWJ1ZmZlckNodW5rcyI6MiwibWF4UXVldWVDaHVua3MiOjN9fQ==");

    CHECK_FALSE (decoded.ok);
    CHECK (decoded.error.containsIgnoreCase ("version"));
}

TEST_CASE ("Prompt state clipboard preserves style card metadata", "[prompt-state]")
{
    mrt::plugin::PromptStateClipboardPayload payload;
    payload.version = 3;
    payload.styleCards.push_back ({
        .id = "card-a",
        .text = "liquid drum and bass",
        .weightPercent = 83.0,
        .active = false,
        .colourArgb = 0xffdb2777,
        .imageKey = "style-backgrounds/liquid-dnb.svg",
        .userCreated = true,
        .bankColumn = 2,
        .bankRow = 1
    });
    payload.settings.transitionDelaySeconds = 4.0;

    const auto encoded = mrt::plugin::encodePromptStateClipboardPayload (payload);
    const auto decoded = mrt::plugin::decodePromptStateClipboardPayload (encoded);

    REQUIRE (decoded.ok);
    REQUIRE (decoded.payload.styleCards.size() == 1);
    CHECK (decoded.payload.styleCards[0].text == "liquid drum and bass");
    CHECK_FALSE (decoded.payload.styleCards[0].active);
    CHECK (decoded.payload.styleCards[0].colourArgb == 0xffdb2777);
    CHECK (decoded.payload.styleCards[0].imageKey == "style-backgrounds/liquid-dnb.svg");
    CHECK (decoded.payload.styleCards[0].bankColumn == 2);
    CHECK (decoded.payload.styleCards[0].bankRow == 1);
    CHECK (decoded.payload.settings.transitionDelaySeconds == Catch::Approx (4.0));
}

TEST_CASE ("Prompt state clipboard rejects invalid payloads", "[prompt-state]")
{
    auto decoded = mrt::plugin::decodePromptStateClipboardPayload ("not base64 json");
    CHECK_FALSE (decoded.ok);
    CHECK_FALSE (decoded.error.isEmpty());

    juce::MemoryBlock wrongVersion;
    const char* wrongVersionJson = "{\"version\":2,\"prompts\":[],\"settings\":{}}";
    wrongVersion.append (wrongVersionJson, std::strlen (wrongVersionJson));
    decoded = mrt::plugin::decodePromptStateClipboardPayload (
        juce::Base64::toBase64 (wrongVersion.getData(), wrongVersion.getSize()));
    CHECK_FALSE (decoded.ok);
    CHECK (decoded.error.containsIgnoreCase ("version"));
}
