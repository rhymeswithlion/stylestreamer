#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "engine/GeneratedAudioQueue.h"

TEST_CASE ("GeneratedAudioQueue drains interleaved chunks into deinterleaved outputs", "[audio-queue]")
{
    mrt::plugin::GeneratedAudioQueue queue (2, 8);
    const float input[] {
        0.1f, 0.2f,
        0.3f, 0.4f,
        0.5f, 0.6f,
    };

    REQUIRE (queue.pushInterleaved (input, 3, 2));

    float left[5] {};
    float right[5] {};
    float* outputs[] { left, right };

    const auto copied = queue.popToDeinterleaved (outputs, 2, 5);

    CHECK (copied == 3);
    CHECK (left[0] == Catch::Approx (0.1f));
    CHECK (left[1] == Catch::Approx (0.3f));
    CHECK (left[2] == Catch::Approx (0.5f));
    CHECK (left[3] == Catch::Approx (0.0f));
    CHECK (right[0] == Catch::Approx (0.2f));
    CHECK (right[1] == Catch::Approx (0.4f));
    CHECK (right[2] == Catch::Approx (0.6f));
    CHECK (right[3] == Catch::Approx (0.0f));
    CHECK (queue.queuedFrames() == 0);
}

TEST_CASE ("GeneratedAudioQueue rejects chunks that exceed remaining capacity", "[audio-queue]")
{
    mrt::plugin::GeneratedAudioQueue queue (2, 4);
    const float input[] {
        0.1f, 0.2f,
        0.3f, 0.4f,
        0.5f, 0.6f,
        0.7f, 0.8f,
        0.9f, 1.0f,
    };

    CHECK_FALSE (queue.pushInterleaved (input, 5, 2));
    CHECK (queue.queuedFrames() == 0);

    REQUIRE (queue.pushInterleaved (input, 3, 2));
    CHECK (queue.queuedFrames() == 3);

    queue.clear();
    CHECK (queue.queuedFrames() == 0);
}
