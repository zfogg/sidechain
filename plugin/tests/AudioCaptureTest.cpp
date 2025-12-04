#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "AudioCapture.h"

using Catch::Matchers::WithinAbs;

//==============================================================================
// Test fixture helper
class AudioCaptureTestFixture
{
public:
    AudioCapture capture;

    void prepareDefault()
    {
        capture.prepare(44100.0, 512, 2);
    }

    juce::AudioBuffer<float> createTestBuffer(int numSamples, float amplitude = 0.5f)
    {
        juce::AudioBuffer<float> buffer(2, numSamples);
        for (int ch = 0; ch < 2; ++ch)
        {
            auto* data = buffer.getWritePointer(ch);
            for (int i = 0; i < numSamples; ++i)
            {
                // Create a simple sine wave for testing
                data[i] = amplitude * std::sin(2.0f * juce::MathConstants<float>::pi * 440.0f * i / 44100.0f);
            }
        }
        return buffer;
    }

    juce::AudioBuffer<float> createSilentBuffer(int numSamples)
    {
        juce::AudioBuffer<float> buffer(2, numSamples);
        buffer.clear();
        return buffer;
    }
};

//==============================================================================
TEST_CASE_METHOD(AudioCaptureTestFixture, "AudioCapture initial state", "[AudioCapture]")
{
    SECTION("starts not recording")
    {
        REQUIRE(capture.isRecording() == false);
    }

    SECTION("initial levels are zero")
    {
        REQUIRE(capture.getPeakLevel(0) == 0.0f);
        REQUIRE(capture.getPeakLevel(1) == 0.0f);
        REQUIRE(capture.getRMSLevel(0) == 0.0f);
        REQUIRE(capture.getRMSLevel(1) == 0.0f);
    }

    SECTION("initial recording length is zero")
    {
        REQUIRE(capture.getRecordingLengthSamples() == 0);
        REQUIRE(capture.getRecordingLengthSeconds() == 0.0);
    }

    SECTION("initial progress is zero")
    {
        REQUIRE(capture.getRecordingProgress() == 0.0f);
    }

    SECTION("buffer is not full after prepare")
    {
        prepareDefault();
        REQUIRE(capture.isBufferFull() == false);
    }
}

//==============================================================================
TEST_CASE_METHOD(AudioCaptureTestFixture, "AudioCapture prepare", "[AudioCapture]")
{
    SECTION("prepare sets sample rate")
    {
        capture.prepare(48000.0, 256, 2);
        REQUIRE(capture.getSampleRate() == 48000.0);
    }

    SECTION("prepare sets channel count")
    {
        capture.prepare(44100.0, 512, 1);
        REQUIRE(capture.getNumChannels() == 1);
    }

    SECTION("prepare sets max recording samples for 60 seconds")
    {
        capture.prepare(44100.0, 512, 2);
        int expectedSamples = static_cast<int>(60.0 * 44100.0);
        REQUIRE(capture.getMaxRecordingSamples() == expectedSamples);
    }

    SECTION("max recording length is 60 seconds")
    {
        capture.prepare(44100.0, 512, 2);
        REQUIRE_THAT(capture.getMaxRecordingLengthSeconds(), WithinAbs(60.0, 0.001));
    }
}

//==============================================================================
TEST_CASE_METHOD(AudioCaptureTestFixture, "AudioCapture recording state", "[AudioCapture]")
{
    prepareDefault();

    SECTION("startRecording sets recording flag")
    {
        capture.startRecording();
        REQUIRE(capture.isRecording() == true);
    }

    SECTION("stopRecording clears recording flag")
    {
        capture.startRecording();
        capture.stopRecording();
        REQUIRE(capture.isRecording() == false);
    }

    SECTION("startRecording with ID")
    {
        capture.startRecording("test-recording-123");
        REQUIRE(capture.isRecording() == true);
    }

    SECTION("stopRecording returns audio buffer")
    {
        capture.startRecording();

        // Capture some audio
        auto testBuffer = createTestBuffer(512);
        capture.captureAudio(testBuffer);

        auto result = capture.stopRecording();
        REQUIRE(result.getNumSamples() > 0);
        REQUIRE(result.getNumChannels() == 2);
    }
}

//==============================================================================
TEST_CASE_METHOD(AudioCaptureTestFixture, "AudioCapture audio capture", "[AudioCapture]")
{
    prepareDefault();

    SECTION("captureAudio ignores input when not recording")
    {
        auto testBuffer = createTestBuffer(512);
        capture.captureAudio(testBuffer);
        REQUIRE(capture.getRecordingLengthSamples() == 0);
    }

    SECTION("captureAudio records audio when recording")
    {
        capture.startRecording();
        auto testBuffer = createTestBuffer(512);
        capture.captureAudio(testBuffer);
        REQUIRE(capture.getRecordingLengthSamples() == 512);
    }

    SECTION("captureAudio accumulates samples")
    {
        capture.startRecording();
        auto testBuffer = createTestBuffer(512);

        capture.captureAudio(testBuffer);
        capture.captureAudio(testBuffer);
        capture.captureAudio(testBuffer);

        REQUIRE(capture.getRecordingLengthSamples() == 1536);
    }

    SECTION("captureAudio stops at max buffer size")
    {
        capture.prepare(44100.0, 44100, 2); // 1 second blocks
        capture.startRecording();

        // Try to record 70 seconds worth (more than 60s max)
        auto testBuffer = createTestBuffer(44100);
        for (int i = 0; i < 70; ++i)
        {
            capture.captureAudio(testBuffer);
        }

        // Should be clamped to 60 seconds
        REQUIRE(capture.getRecordingLengthSamples() <= capture.getMaxRecordingSamples());
        REQUIRE(capture.isBufferFull() == true);
    }
}

//==============================================================================
TEST_CASE_METHOD(AudioCaptureTestFixture, "AudioCapture level metering", "[AudioCapture]")
{
    prepareDefault();
    capture.startRecording();

    SECTION("silent audio produces zero levels")
    {
        auto silentBuffer = createSilentBuffer(2048);
        capture.captureAudio(silentBuffer);

        // Peak should be 0 for silent audio
        REQUIRE(capture.getPeakLevel(0) == 0.0f);
        REQUIRE(capture.getPeakLevel(1) == 0.0f);
    }

    SECTION("audio produces non-zero peak levels")
    {
        auto testBuffer = createTestBuffer(2048, 0.8f);
        capture.captureAudio(testBuffer);

        // Should detect peaks from the sine wave
        REQUIRE(capture.getPeakLevel(0) > 0.0f);
        REQUIRE(capture.getPeakLevel(1) > 0.0f);
    }

    SECTION("peak level is bounded by amplitude")
    {
        auto testBuffer = createTestBuffer(2048, 0.5f);
        capture.captureAudio(testBuffer);

        // Peak should not exceed the amplitude
        REQUIRE(capture.getPeakLevel(0) <= 0.5f);
        REQUIRE(capture.getPeakLevel(1) <= 0.5f);
    }

    SECTION("resetLevels clears levels")
    {
        auto testBuffer = createTestBuffer(2048, 0.8f);
        capture.captureAudio(testBuffer);

        capture.resetLevels();

        REQUIRE(capture.getPeakLevel(0) == 0.0f);
        REQUIRE(capture.getPeakLevel(1) == 0.0f);
        REQUIRE(capture.getRMSLevel(0) == 0.0f);
        REQUIRE(capture.getRMSLevel(1) == 0.0f);
    }
}

//==============================================================================
TEST_CASE_METHOD(AudioCaptureTestFixture, "AudioCapture progress calculation", "[AudioCapture]")
{
    prepareDefault();
    capture.startRecording();

    SECTION("progress starts at zero")
    {
        REQUIRE(capture.getRecordingProgress() == 0.0f);
    }

    SECTION("progress increases with recorded audio")
    {
        auto testBuffer = createTestBuffer(44100); // 1 second
        capture.captureAudio(testBuffer);

        float progress = capture.getRecordingProgress();
        REQUIRE(progress > 0.0f);
        REQUIRE(progress < 1.0f);
    }

    SECTION("recording length seconds is accurate")
    {
        auto testBuffer = createTestBuffer(44100); // 1 second at 44100 Hz
        capture.captureAudio(testBuffer);

        REQUIRE_THAT(capture.getRecordingLengthSeconds(), WithinAbs(1.0, 0.01));
    }
}

//==============================================================================
TEST_CASE_METHOD(AudioCaptureTestFixture, "AudioCapture reset", "[AudioCapture]")
{
    prepareDefault();
    capture.startRecording();

    auto testBuffer = createTestBuffer(4096);
    capture.captureAudio(testBuffer);
    capture.stopRecording();

    SECTION("reset clears recording state")
    {
        capture.reset();
        REQUIRE(capture.isRecording() == false);
        REQUIRE(capture.getRecordingLengthSamples() == 0);
    }

    SECTION("reset clears levels")
    {
        capture.reset();
        REQUIRE(capture.getPeakLevel(0) == 0.0f);
        REQUIRE(capture.getPeakLevel(1) == 0.0f);
    }
}

//==============================================================================
TEST_CASE_METHOD(AudioCaptureTestFixture, "AudioCapture waveform generation", "[AudioCapture]")
{
    SECTION("generateWaveformSVG produces valid SVG")
    {
        juce::AudioBuffer<float> buffer(2, 1024);
        for (int i = 0; i < 1024; ++i)
        {
            float sample = std::sin(2.0f * juce::MathConstants<float>::pi * i / 100.0f);
            buffer.setSample(0, i, sample);
            buffer.setSample(1, i, sample);
        }

        juce::String svg = capture.generateWaveformSVG(buffer, 200, 50);

        REQUIRE(svg.startsWith("<svg"));
        REQUIRE(svg.endsWith("</svg>"));
        REQUIRE(svg.contains("width=\"200\""));
        REQUIRE(svg.contains("height=\"50\""));
    }

    SECTION("generateWaveformSVG handles empty buffer")
    {
        juce::AudioBuffer<float> emptyBuffer;
        juce::String svg = capture.generateWaveformSVG(emptyBuffer);

        // Empty buffer returns empty string
        REQUIRE(svg.isEmpty());
    }
}
