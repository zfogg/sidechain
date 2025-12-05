#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "audio/AudioCapture.h"

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

//==============================================================================
TEST_CASE_METHOD(AudioCaptureTestFixture, "AudioCapture WAV export", "[AudioCapture][Export]")
{
    prepareDefault();

    SECTION("getTempAudioFile returns valid path")
    {
        auto tempFile = AudioCapture::getTempAudioFile(".wav");

        REQUIRE(tempFile.getFileExtension() == ".wav");
        REQUIRE(tempFile.getFileName().startsWith("sidechain_"));
        REQUIRE(tempFile.getParentDirectory().getFileName() == "Sidechain");
    }

    SECTION("getTempAudioFile generates unique names")
    {
        auto file1 = AudioCapture::getTempAudioFile(".wav");
        auto file2 = AudioCapture::getTempAudioFile(".wav");

        REQUIRE(file1.getFullPathName() != file2.getFullPathName());
    }

    SECTION("getTempAudioFile supports custom extensions")
    {
        auto flacFile = AudioCapture::getTempAudioFile(".flac");
        REQUIRE(flacFile.getFileExtension() == ".flac");

        auto mp3File = AudioCapture::getTempAudioFile(".mp3");
        REQUIRE(mp3File.getFileExtension() == ".mp3");
    }

    SECTION("hasRecordedAudio returns false initially")
    {
        REQUIRE(capture.hasRecordedAudio() == false);
    }

    SECTION("hasRecordedAudio returns true after recording")
    {
        capture.startRecording();
        auto testBuffer = createTestBuffer(4096);
        capture.captureAudio(testBuffer);
        capture.stopRecording();

        REQUIRE(capture.hasRecordedAudio() == true);
    }

    SECTION("getRecordedAudioBuffer returns captured audio")
    {
        capture.startRecording();
        auto testBuffer = createTestBuffer(2048);
        capture.captureAudio(testBuffer);
        capture.stopRecording();

        const auto& recordedBuffer = capture.getRecordedAudioBuffer();
        REQUIRE(recordedBuffer.getNumSamples() == 2048);
        REQUIRE(recordedBuffer.getNumChannels() == 2);
    }

    SECTION("saveBufferToWavFile saves valid WAV file")
    {
        auto testBuffer = createTestBuffer(8820); // 0.2 seconds at 44100 Hz
        auto tempFile = AudioCapture::getTempAudioFile(".wav");

        bool success = AudioCapture::saveBufferToWavFile(
            tempFile, testBuffer, 44100.0, AudioCapture::ExportFormat::WAV_16bit);

        REQUIRE(success == true);
        REQUIRE(tempFile.exists() == true);
        REQUIRE(tempFile.getSize() > 0);

        // Cleanup
        tempFile.deleteFile();
    }

    SECTION("saveBufferToWavFile supports different bit depths")
    {
        auto testBuffer = createTestBuffer(4410);

        // Test 16-bit
        auto file16 = AudioCapture::getTempAudioFile(".wav");
        REQUIRE(AudioCapture::saveBufferToWavFile(
            file16, testBuffer, 44100.0, AudioCapture::ExportFormat::WAV_16bit));
        auto size16 = file16.getSize();
        file16.deleteFile();

        // Test 24-bit
        auto file24 = AudioCapture::getTempAudioFile(".wav");
        REQUIRE(AudioCapture::saveBufferToWavFile(
            file24, testBuffer, 44100.0, AudioCapture::ExportFormat::WAV_24bit));
        auto size24 = file24.getSize();
        file24.deleteFile();

        // 24-bit should be larger than 16-bit for same audio
        REQUIRE(size24 > size16);
    }

    SECTION("saveBufferToWavFile fails with empty buffer")
    {
        juce::AudioBuffer<float> emptyBuffer;
        auto tempFile = AudioCapture::getTempAudioFile(".wav");

        bool success = AudioCapture::saveBufferToWavFile(
            tempFile, emptyBuffer, 44100.0);

        REQUIRE(success == false);
    }

    SECTION("saveBufferToWavFile fails with invalid sample rate")
    {
        auto testBuffer = createTestBuffer(1024);
        auto tempFile = AudioCapture::getTempAudioFile(".wav");

        bool success = AudioCapture::saveBufferToWavFile(
            tempFile, testBuffer, 0.0);

        REQUIRE(success == false);
    }

    SECTION("saveRecordedAudioToWavFile saves recorded audio")
    {
        capture.startRecording();
        auto testBuffer = createTestBuffer(8820);
        capture.captureAudio(testBuffer);
        capture.stopRecording();

        auto tempFile = AudioCapture::getTempAudioFile(".wav");
        bool success = capture.saveRecordedAudioToWavFile(tempFile);

        REQUIRE(success == true);
        REQUIRE(tempFile.exists() == true);

        // Cleanup
        tempFile.deleteFile();
    }

    SECTION("saveRecordedAudioToWavFile fails with no recorded audio")
    {
        auto tempFile = AudioCapture::getTempAudioFile(".wav");
        bool success = capture.saveRecordedAudioToWavFile(tempFile);

        REQUIRE(success == false);
    }
}

//==============================================================================
TEST_CASE_METHOD(AudioCaptureTestFixture, "AudioCapture FLAC export", "[AudioCapture][Export][FLAC]")
{
    prepareDefault();

    SECTION("isFlacFormat correctly identifies FLAC formats")
    {
        REQUIRE(AudioCapture::isFlacFormat(AudioCapture::ExportFormat::FLAC_16bit) == true);
        REQUIRE(AudioCapture::isFlacFormat(AudioCapture::ExportFormat::FLAC_24bit) == true);
        REQUIRE(AudioCapture::isFlacFormat(AudioCapture::ExportFormat::WAV_16bit) == false);
        REQUIRE(AudioCapture::isFlacFormat(AudioCapture::ExportFormat::WAV_24bit) == false);
        REQUIRE(AudioCapture::isFlacFormat(AudioCapture::ExportFormat::WAV_32bit) == false);
    }

    SECTION("getExtensionForFormat returns correct extensions")
    {
        REQUIRE(AudioCapture::getExtensionForFormat(AudioCapture::ExportFormat::WAV_16bit) == ".wav");
        REQUIRE(AudioCapture::getExtensionForFormat(AudioCapture::ExportFormat::WAV_24bit) == ".wav");
        REQUIRE(AudioCapture::getExtensionForFormat(AudioCapture::ExportFormat::WAV_32bit) == ".wav");
        REQUIRE(AudioCapture::getExtensionForFormat(AudioCapture::ExportFormat::FLAC_16bit) == ".flac");
        REQUIRE(AudioCapture::getExtensionForFormat(AudioCapture::ExportFormat::FLAC_24bit) == ".flac");
    }

    SECTION("saveBufferToFlacFile saves valid FLAC file")
    {
        auto testBuffer = createTestBuffer(8820); // 0.2 seconds
        auto tempFile = AudioCapture::getTempAudioFile(".flac");

        bool success = AudioCapture::saveBufferToFlacFile(
            tempFile, testBuffer, 44100.0, AudioCapture::ExportFormat::FLAC_16bit);

        REQUIRE(success == true);
        REQUIRE(tempFile.exists() == true);
        REQUIRE(tempFile.getSize() > 0);

        // Cleanup
        tempFile.deleteFile();
    }

    SECTION("FLAC is smaller than WAV for same audio")
    {
        auto testBuffer = createTestBuffer(44100); // 1 second

        auto wavFile = AudioCapture::getTempAudioFile(".wav");
        REQUIRE(AudioCapture::saveBufferToWavFile(
            wavFile, testBuffer, 44100.0, AudioCapture::ExportFormat::WAV_16bit));
        auto wavSize = wavFile.getSize();

        auto flacFile = AudioCapture::getTempAudioFile(".flac");
        REQUIRE(AudioCapture::saveBufferToFlacFile(
            flacFile, testBuffer, 44100.0, AudioCapture::ExportFormat::FLAC_16bit));
        auto flacSize = flacFile.getSize();

        // FLAC should be smaller than WAV due to lossless compression
        REQUIRE(flacSize < wavSize);

        // Cleanup
        wavFile.deleteFile();
        flacFile.deleteFile();
    }

    SECTION("saveBufferToFile routes to correct format")
    {
        auto testBuffer = createTestBuffer(4410);

        // Test WAV routing
        auto wavFile = AudioCapture::getTempAudioFile(".wav");
        REQUIRE(AudioCapture::saveBufferToFile(
            wavFile, testBuffer, 44100.0, AudioCapture::ExportFormat::WAV_16bit));
        REQUIRE(wavFile.exists());
        wavFile.deleteFile();

        // Test FLAC routing
        auto flacFile = AudioCapture::getTempAudioFile(".flac");
        REQUIRE(AudioCapture::saveBufferToFile(
            flacFile, testBuffer, 44100.0, AudioCapture::ExportFormat::FLAC_16bit));
        REQUIRE(flacFile.exists());
        flacFile.deleteFile();
    }

    SECTION("saveBufferToFlacFile supports different bit depths")
    {
        auto testBuffer = createTestBuffer(4410);

        // 16-bit FLAC
        auto file16 = AudioCapture::getTempAudioFile(".flac");
        REQUIRE(AudioCapture::saveBufferToFlacFile(
            file16, testBuffer, 44100.0, AudioCapture::ExportFormat::FLAC_16bit));
        auto size16 = file16.getSize();
        file16.deleteFile();

        // 24-bit FLAC
        auto file24 = AudioCapture::getTempAudioFile(".flac");
        REQUIRE(AudioCapture::saveBufferToFlacFile(
            file24, testBuffer, 44100.0, AudioCapture::ExportFormat::FLAC_24bit));
        auto size24 = file24.getSize();
        file24.deleteFile();

        // 24-bit should generally be larger
        REQUIRE(size24 >= size16);
    }

    SECTION("saveRecordedAudioToFile saves recorded audio as FLAC")
    {
        capture.startRecording();
        auto testBuffer = createTestBuffer(8820);
        capture.captureAudio(testBuffer);
        capture.stopRecording();

        auto tempFile = AudioCapture::getTempAudioFile(".flac");
        bool success = capture.saveRecordedAudioToFile(tempFile, AudioCapture::ExportFormat::FLAC_16bit);

        REQUIRE(success == true);
        REQUIRE(tempFile.exists() == true);

        // Cleanup
        tempFile.deleteFile();
    }

    SECTION("saveBufferToFlacFile fails with empty buffer")
    {
        juce::AudioBuffer<float> emptyBuffer;
        auto tempFile = AudioCapture::getTempAudioFile(".flac");

        bool success = AudioCapture::saveBufferToFlacFile(
            tempFile, emptyBuffer, 44100.0);

        REQUIRE(success == false);
    }
}

//==============================================================================
TEST_CASE("AudioCapture duration formatting", "[AudioCapture][Utilities]")
{
    SECTION("formatDuration formats zero correctly")
    {
        REQUIRE(AudioCapture::formatDuration(0.0) == "0:00");
    }

    SECTION("formatDuration formats seconds correctly")
    {
        REQUIRE(AudioCapture::formatDuration(5.0) == "0:05");
        REQUIRE(AudioCapture::formatDuration(30.0) == "0:30");
        REQUIRE(AudioCapture::formatDuration(59.0) == "0:59");
    }

    SECTION("formatDuration formats minutes correctly")
    {
        REQUIRE(AudioCapture::formatDuration(60.0) == "1:00");
        REQUIRE(AudioCapture::formatDuration(61.0) == "1:01");
        REQUIRE(AudioCapture::formatDuration(90.0) == "1:30");
        REQUIRE(AudioCapture::formatDuration(125.0) == "2:05");
    }

    SECTION("formatDuration truncates fractional seconds")
    {
        REQUIRE(AudioCapture::formatDuration(5.9) == "0:05");
        REQUIRE(AudioCapture::formatDuration(59.999) == "0:59");
    }

    SECTION("formatDuration handles negative values as zero")
    {
        REQUIRE(AudioCapture::formatDuration(-5.0) == "0:00");
    }

    SECTION("formatDurationWithMs includes milliseconds")
    {
        REQUIRE(AudioCapture::formatDurationWithMs(0.0) == "0:00.000");
        REQUIRE(AudioCapture::formatDurationWithMs(5.123) == "0:05.123");
        REQUIRE(AudioCapture::formatDurationWithMs(61.5) == "1:01.500");
    }

    SECTION("formatDurationWithMs handles edge cases")
    {
        REQUIRE(AudioCapture::formatDurationWithMs(0.001) == "0:00.001");
        REQUIRE(AudioCapture::formatDurationWithMs(0.999) == "0:00.999");
    }
}

//==============================================================================
TEST_CASE("AudioCapture file size utilities", "[AudioCapture][Utilities]")
{
    SECTION("formatFileSize formats bytes correctly")
    {
        REQUIRE(AudioCapture::formatFileSize(0) == "0 bytes");
        REQUIRE(AudioCapture::formatFileSize(500) == "500 bytes");
        REQUIRE(AudioCapture::formatFileSize(1023) == "1023 bytes");
    }

    SECTION("formatFileSize formats kilobytes correctly")
    {
        REQUIRE(AudioCapture::formatFileSize(1024) == "1.0 KB");
        REQUIRE(AudioCapture::formatFileSize(1536) == "1.5 KB");
        REQUIRE(AudioCapture::formatFileSize(10240) == "10.0 KB");
    }

    SECTION("formatFileSize formats megabytes correctly")
    {
        REQUIRE(AudioCapture::formatFileSize(1024 * 1024) == "1.00 MB");
        REQUIRE(AudioCapture::formatFileSize(1024 * 1024 * 5) == "5.00 MB");
        REQUIRE(AudioCapture::formatFileSize(1024 * 1024 + 512 * 1024) == "1.50 MB");
    }

    SECTION("formatFileSize formats gigabytes correctly")
    {
        juce::int64 oneGB = 1024LL * 1024 * 1024;
        REQUIRE(AudioCapture::formatFileSize(oneGB) == "1.00 GB");
        REQUIRE(AudioCapture::formatFileSize(oneGB * 2) == "2.00 GB");
    }

    SECTION("formatFileSize handles negative values as zero")
    {
        REQUIRE(AudioCapture::formatFileSize(-100) == "0 bytes");
    }

    SECTION("estimateFileSize returns zero for invalid input")
    {
        REQUIRE(AudioCapture::estimateFileSize(0, 2, AudioCapture::ExportFormat::WAV_16bit) == 0);
        REQUIRE(AudioCapture::estimateFileSize(1000, 0, AudioCapture::ExportFormat::WAV_16bit) == 0);
        REQUIRE(AudioCapture::estimateFileSize(-1, 2, AudioCapture::ExportFormat::WAV_16bit) == 0);
    }

    SECTION("estimateFileSize calculates WAV sizes correctly")
    {
        // 1 second of stereo 44100Hz audio at 16-bit = 44100 * 2 * 2 = 176,400 bytes + 44 header
        juce::int64 expected = 44100 * 2 * 2 + 44;
        REQUIRE(AudioCapture::estimateFileSize(44100, 2, AudioCapture::ExportFormat::WAV_16bit) == expected);
    }

    SECTION("estimateFileSize WAV 24-bit is larger than 16-bit")
    {
        auto size16 = AudioCapture::estimateFileSize(44100, 2, AudioCapture::ExportFormat::WAV_16bit);
        auto size24 = AudioCapture::estimateFileSize(44100, 2, AudioCapture::ExportFormat::WAV_24bit);
        auto size32 = AudioCapture::estimateFileSize(44100, 2, AudioCapture::ExportFormat::WAV_32bit);

        REQUIRE(size24 > size16);
        REQUIRE(size32 > size24);
    }

    SECTION("estimateFileSize FLAC is smaller than WAV")
    {
        auto wavSize = AudioCapture::estimateFileSize(44100, 2, AudioCapture::ExportFormat::WAV_16bit);
        auto flacSize = AudioCapture::estimateFileSize(44100, 2, AudioCapture::ExportFormat::FLAC_16bit);

        REQUIRE(flacSize < wavSize);
    }
}

//==============================================================================
TEST_CASE_METHOD(AudioCaptureTestFixture, "AudioCapture estimated file size", "[AudioCapture][Utilities]")
{
    prepareDefault();

    SECTION("getEstimatedFileSize returns zero with no recorded audio")
    {
        REQUIRE(capture.getEstimatedFileSize(AudioCapture::ExportFormat::WAV_16bit) == 0);
    }

    SECTION("getEstimatedFileSize returns size after recording")
    {
        capture.startRecording();
        auto testBuffer = createTestBuffer(44100); // 1 second
        capture.captureAudio(testBuffer);
        capture.stopRecording();

        auto estimatedSize = capture.getEstimatedFileSize(AudioCapture::ExportFormat::WAV_16bit);

        // Should be approximately 1 second of stereo 16-bit audio (~176KB)
        REQUIRE(estimatedSize > 170000);
        REQUIRE(estimatedSize < 180000);
    }
}

//==============================================================================
TEST_CASE("AudioCapture trim operations", "[AudioCapture][Processing]")
{
    // Create a test buffer with known values: channel 0 has sample index as value
    auto createIndexedBuffer = [](int numSamples) {
        juce::AudioBuffer<float> buffer(2, numSamples);
        for (int i = 0; i < numSamples; ++i)
        {
            buffer.setSample(0, i, static_cast<float>(i));
            buffer.setSample(1, i, static_cast<float>(i) * 0.5f);
        }
        return buffer;
    };

    SECTION("trimBuffer extracts correct range")
    {
        auto buffer = createIndexedBuffer(1000);
        auto trimmed = AudioCapture::trimBuffer(buffer, 100, 200);

        REQUIRE(trimmed.getNumSamples() == 100);
        REQUIRE(trimmed.getNumChannels() == 2);
        REQUIRE(trimmed.getSample(0, 0) == 100.0f);
        REQUIRE(trimmed.getSample(0, 99) == 199.0f);
    }

    SECTION("trimBuffer with -1 end trims to end of buffer")
    {
        auto buffer = createIndexedBuffer(1000);
        auto trimmed = AudioCapture::trimBuffer(buffer, 900, -1);

        REQUIRE(trimmed.getNumSamples() == 100);
        REQUIRE(trimmed.getSample(0, 0) == 900.0f);
        REQUIRE(trimmed.getSample(0, 99) == 999.0f);
    }

    SECTION("trimBuffer with invalid range returns empty buffer")
    {
        auto buffer = createIndexedBuffer(1000);

        // Start >= end
        auto trimmed1 = AudioCapture::trimBuffer(buffer, 500, 500);
        REQUIRE(trimmed1.getNumSamples() == 0);

        // Start > end
        auto trimmed2 = AudioCapture::trimBuffer(buffer, 600, 400);
        REQUIRE(trimmed2.getNumSamples() == 0);
    }

    SECTION("trimBuffer clamps out-of-bounds indices")
    {
        auto buffer = createIndexedBuffer(1000);

        // Start negative
        auto trimmed1 = AudioCapture::trimBuffer(buffer, -100, 100);
        REQUIRE(trimmed1.getNumSamples() == 100);
        REQUIRE(trimmed1.getSample(0, 0) == 0.0f);

        // End past buffer
        auto trimmed2 = AudioCapture::trimBuffer(buffer, 900, 2000);
        REQUIRE(trimmed2.getNumSamples() == 100);
    }

    SECTION("trimBufferByTime converts seconds to samples")
    {
        auto buffer = createIndexedBuffer(44100); // 1 second at 44100Hz
        auto trimmed = AudioCapture::trimBufferByTime(buffer, 44100.0, 0.5, 0.75);

        // 0.5s to 0.75s = 22050 to 33075 samples = 11025 samples
        REQUIRE(trimmed.getNumSamples() == 11025);
    }

    SECTION("trimBufferByTime with -1.0 end trims to end")
    {
        auto buffer = createIndexedBuffer(44100);
        auto trimmed = AudioCapture::trimBufferByTime(buffer, 44100.0, 0.9, -1.0);

        // 0.9s to end = 39690 to 44100 = 4410 samples
        REQUIRE(trimmed.getNumSamples() == 4410);
    }

    SECTION("trimBufferByTime with invalid sample rate returns empty")
    {
        auto buffer = createIndexedBuffer(1000);
        auto trimmed = AudioCapture::trimBufferByTime(buffer, 0.0, 0.0, 0.5);
        REQUIRE(trimmed.getNumSamples() == 0);
    }
}

//==============================================================================
TEST_CASE("AudioCapture fade operations", "[AudioCapture][Processing]")
{
    // Create a buffer filled with 1.0
    auto createOnesBuffer = [](int numSamples, int numChannels = 2) {
        juce::AudioBuffer<float> buffer(numChannels, numSamples);
        for (int ch = 0; ch < numChannels; ++ch)
        {
            for (int i = 0; i < numSamples; ++i)
            {
                buffer.setSample(ch, i, 1.0f);
            }
        }
        return buffer;
    };

    SECTION("applyFadeIn linear fade starts at zero and ends at full")
    {
        auto buffer = createOnesBuffer(1000);
        AudioCapture::applyFadeIn(buffer, 100, AudioCapture::FadeType::Linear);

        // First sample should be 0 (or very close)
        REQUIRE(buffer.getSample(0, 0) < 0.01f);

        // Last sample of fade should be close to 1.0
        REQUIRE(buffer.getSample(0, 99) > 0.98f);

        // After fade region should be untouched
        REQUIRE(buffer.getSample(0, 100) == 1.0f);
        REQUIRE(buffer.getSample(0, 500) == 1.0f);
    }

    SECTION("applyFadeOut linear fade starts at full and ends at zero")
    {
        auto buffer = createOnesBuffer(1000);
        AudioCapture::applyFadeOut(buffer, 100, AudioCapture::FadeType::Linear);

        // Before fade region should be untouched
        REQUIRE(buffer.getSample(0, 0) == 1.0f);
        REQUIRE(buffer.getSample(0, 899) == 1.0f);

        // First sample of fade (at 900) should be close to 1.0
        REQUIRE(buffer.getSample(0, 900) > 0.98f);

        // Last sample should be 0 (or very close)
        REQUIRE(buffer.getSample(0, 999) < 0.02f);
    }

    SECTION("applyFadeIn exponential fade has slower start")
    {
        auto buffer = createOnesBuffer(1000);
        AudioCapture::applyFadeIn(buffer, 100, AudioCapture::FadeType::Exponential);

        // Exponential: gain = position^2, so at 50% position, gain should be 0.25
        float midpointGain = buffer.getSample(0, 50);
        REQUIRE(midpointGain < 0.35f); // Should be closer to 0.25 than 0.5
    }

    SECTION("applyFadeIn SCurve fade has smooth transitions")
    {
        auto buffer = createOnesBuffer(1000);
        AudioCapture::applyFadeIn(buffer, 100, AudioCapture::FadeType::SCurve);

        // S-curve: at 50% position, gain should be 0.5
        float midpointGain = buffer.getSample(0, 50);
        REQUIRE_THAT(midpointGain, WithinAbs(0.5f, 0.05f));
    }

    SECTION("applyFadeIn handles fade longer than buffer")
    {
        auto buffer = createOnesBuffer(100);
        AudioCapture::applyFadeIn(buffer, 1000, AudioCapture::FadeType::Linear);

        // Should clamp to buffer length
        REQUIRE(buffer.getSample(0, 0) < 0.01f);
        REQUIRE(buffer.getSample(0, 99) > 0.98f);
    }

    SECTION("applyFadeOut handles fade longer than buffer")
    {
        auto buffer = createOnesBuffer(100);
        AudioCapture::applyFadeOut(buffer, 1000, AudioCapture::FadeType::Linear);

        // Should clamp to buffer length - entire buffer is fade
        REQUIRE(buffer.getSample(0, 0) > 0.98f);
        REQUIRE(buffer.getSample(0, 99) < 0.02f);
    }

    SECTION("applyFadeIn with zero samples does nothing")
    {
        auto buffer = createOnesBuffer(100);
        AudioCapture::applyFadeIn(buffer, 0, AudioCapture::FadeType::Linear);

        REQUIRE(buffer.getSample(0, 0) == 1.0f);
    }

    SECTION("applyFadeInByTime converts time to samples")
    {
        auto buffer = createOnesBuffer(44100);
        AudioCapture::applyFadeInByTime(buffer, 44100.0, 0.05, AudioCapture::FadeType::Linear);

        // 50ms at 44100Hz = 2205 samples
        // After fade should be untouched
        REQUIRE(buffer.getSample(0, 2205) == 1.0f);
    }

    SECTION("applyFadeOutByTime converts time to samples")
    {
        auto buffer = createOnesBuffer(44100);
        AudioCapture::applyFadeOutByTime(buffer, 44100.0, 0.05, AudioCapture::FadeType::Linear);

        // Before fade should be untouched
        REQUIRE(buffer.getSample(0, 0) == 1.0f);
        REQUIRE(buffer.getSample(0, 44100 - 2206) == 1.0f);
    }

    SECTION("fade applies to all channels")
    {
        auto buffer = createOnesBuffer(1000, 4);
        AudioCapture::applyFadeIn(buffer, 100, AudioCapture::FadeType::Linear);

        // All channels should have same fade applied
        for (int ch = 0; ch < 4; ++ch)
        {
            REQUIRE(buffer.getSample(ch, 0) < 0.01f);
            REQUIRE(buffer.getSample(ch, 50) > 0.45f);
            REQUIRE(buffer.getSample(ch, 100) == 1.0f);
        }
    }
}

//==============================================================================
TEST_CASE("AudioCapture normalization", "[AudioCapture][Processing]")
{
    SECTION("dbToLinear converts correctly")
    {
        REQUIRE_THAT(AudioCapture::dbToLinear(0.0f), WithinAbs(1.0f, 0.0001f));
        REQUIRE_THAT(AudioCapture::dbToLinear(-6.0f), WithinAbs(0.5012f, 0.01f));
        REQUIRE_THAT(AudioCapture::dbToLinear(-20.0f), WithinAbs(0.1f, 0.001f));
        REQUIRE_THAT(AudioCapture::dbToLinear(6.0f), WithinAbs(1.995f, 0.01f));
    }

    SECTION("linearToDb converts correctly")
    {
        REQUIRE_THAT(AudioCapture::linearToDb(1.0f), WithinAbs(0.0f, 0.0001f));
        REQUIRE_THAT(AudioCapture::linearToDb(0.5f), WithinAbs(-6.02f, 0.1f));
        REQUIRE_THAT(AudioCapture::linearToDb(0.1f), WithinAbs(-20.0f, 0.1f));
    }

    SECTION("linearToDb returns -inf for zero")
    {
        float result = AudioCapture::linearToDb(0.0f);
        REQUIRE(std::isinf(result));
        REQUIRE(result < 0);
    }

    SECTION("getBufferPeakLevel finds maximum sample")
    {
        juce::AudioBuffer<float> buffer(2, 1000);
        buffer.clear();
        buffer.setSample(0, 500, 0.8f);
        buffer.setSample(1, 300, -0.9f);  // Negative peak

        float peak = AudioCapture::getBufferPeakLevel(buffer);
        REQUIRE_THAT(peak, WithinAbs(0.9f, 0.0001f));
    }

    SECTION("getBufferPeakLevel returns zero for empty buffer")
    {
        juce::AudioBuffer<float> buffer;
        REQUIRE(AudioCapture::getBufferPeakLevel(buffer) == 0.0f);
    }

    SECTION("getBufferPeakLevelDB returns peak in decibels")
    {
        juce::AudioBuffer<float> buffer(1, 100);
        buffer.clear();
        buffer.setSample(0, 50, 0.5f);

        float peakDB = AudioCapture::getBufferPeakLevelDB(buffer);
        REQUIRE_THAT(peakDB, WithinAbs(-6.02f, 0.1f));
    }

    SECTION("normalizeBuffer adjusts to target peak")
    {
        juce::AudioBuffer<float> buffer(2, 1000);
        buffer.clear();
        buffer.setSample(0, 500, 0.5f);
        buffer.setSample(1, 300, -0.5f);

        // Normalize to -1dB
        float gain = AudioCapture::normalizeBuffer(buffer, -1.0f);

        // Check that peak is now at -1dB (0.891 linear)
        float newPeak = AudioCapture::getBufferPeakLevel(buffer);
        REQUIRE_THAT(newPeak, WithinAbs(0.891f, 0.01f));

        // Gain should be about +5dB (from -6dB to -1dB)
        float gainDB = AudioCapture::linearToDb(gain);
        REQUIRE_THAT(gainDB, WithinAbs(5.0f, 0.2f));
    }

    SECTION("normalizeBuffer to 0dB")
    {
        juce::AudioBuffer<float> buffer(1, 100);
        buffer.clear();
        buffer.setSample(0, 50, 0.25f);

        AudioCapture::normalizeBuffer(buffer, 0.0f);

        float newPeak = AudioCapture::getBufferPeakLevel(buffer);
        REQUIRE_THAT(newPeak, WithinAbs(1.0f, 0.001f));
    }

    SECTION("normalizeBuffer returns 1.0 for silent buffer")
    {
        juce::AudioBuffer<float> buffer(1, 100);
        buffer.clear();

        float gain = AudioCapture::normalizeBuffer(buffer, -1.0f);
        REQUIRE(gain == 1.0f);
    }

    SECTION("normalizeBuffer handles negative peaks correctly")
    {
        juce::AudioBuffer<float> buffer(1, 100);
        buffer.clear();
        buffer.setSample(0, 50, -0.8f);

        AudioCapture::normalizeBuffer(buffer, -1.0f);

        // Peak should now be at target
        float newPeak = AudioCapture::getBufferPeakLevel(buffer);
        REQUIRE_THAT(newPeak, WithinAbs(0.891f, 0.01f));
    }

    SECTION("normalizeBuffer with hot signal (> 0dB) reduces gain")
    {
        juce::AudioBuffer<float> buffer(1, 100);
        buffer.clear();
        buffer.setSample(0, 50, 1.5f);  // Clipped/hot signal

        float gain = AudioCapture::normalizeBuffer(buffer, -1.0f);

        // Gain should be less than 1.0 (reducing level)
        REQUIRE(gain < 1.0f);

        float newPeak = AudioCapture::getBufferPeakLevel(buffer);
        REQUIRE_THAT(newPeak, WithinAbs(0.891f, 0.01f));
    }
}

//==============================================================================
TEST_CASE("AudioCapture processing pipeline", "[AudioCapture][Processing][Integration]")
{
    // Test a realistic processing pipeline: trim -> fade -> normalize

    // Create a 2-second buffer with a sine wave
    auto createSineBuffer = [](int numSamples, float amplitude) {
        juce::AudioBuffer<float> buffer(2, numSamples);
        for (int ch = 0; ch < 2; ++ch)
        {
            for (int i = 0; i < numSamples; ++i)
            {
                float sample = amplitude * std::sin(2.0f * juce::MathConstants<float>::pi * 440.0f * i / 44100.0f);
                buffer.setSample(ch, i, sample);
            }
        }
        return buffer;
    };

    SECTION("trim + fade + normalize pipeline")
    {
        // Create 2 seconds of audio at 44100Hz
        auto buffer = createSineBuffer(88200, 0.3f);

        // Step 1: Trim to 1 second (middle portion)
        auto trimmed = AudioCapture::trimBufferByTime(buffer, 44100.0, 0.5, 1.5);
        REQUIRE(trimmed.getNumSamples() == 44100);

        // Step 2: Apply 50ms fades
        AudioCapture::applyFadeInByTime(trimmed, 44100.0, 0.05, AudioCapture::FadeType::Linear);
        AudioCapture::applyFadeOutByTime(trimmed, 44100.0, 0.05, AudioCapture::FadeType::Linear);

        // Step 3: Normalize to -1dB
        AudioCapture::normalizeBuffer(trimmed, -1.0f);

        // Verify final peak is at target
        float finalPeakDB = AudioCapture::getBufferPeakLevelDB(trimmed);
        REQUIRE_THAT(finalPeakDB, WithinAbs(-1.0f, 0.1f));

        // Verify fades are applied (start should be lower than middle)
        float startLevel = std::abs(trimmed.getSample(0, 0));
        float middleLevel = std::abs(trimmed.getSample(0, 22050));
        REQUIRE(startLevel < middleLevel * 0.5f);
    }

    SECTION("export after processing")
    {
        auto buffer = createSineBuffer(44100, 0.5f);

        // Process
        AudioCapture::applyFadeIn(buffer, 2205, AudioCapture::FadeType::Linear);
        AudioCapture::applyFadeOut(buffer, 2205, AudioCapture::FadeType::Linear);
        AudioCapture::normalizeBuffer(buffer, -1.0f);

        // Export to WAV
        auto tempFile = AudioCapture::getTempAudioFile(".wav");
        bool success = AudioCapture::saveBufferToWavFile(tempFile, buffer, 44100.0);

        REQUIRE(success == true);
        REQUIRE(tempFile.exists() == true);
        REQUIRE(tempFile.getSize() > 0);

        tempFile.deleteFile();
    }
}
