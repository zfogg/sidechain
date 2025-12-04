#include "AudioCapture.h"

//==============================================================================
AudioCapture::AudioCapture()
{
}

AudioCapture::~AudioCapture()
{
}

//==============================================================================
void AudioCapture::prepare(double sampleRate, int samplesPerBlock, int numChannels)
{
    currentSampleRate = sampleRate;
    currentNumChannels = juce::jmin(numChannels, MaxChannels);

    // Initialize buffers for 60 seconds of audio maximum
    maxRecordingSamples = static_cast<int>(sampleRate * 60.0);

    initializeBuffers();
    resetLevels();

    DBG("AudioCapture prepared: " + juce::String(sampleRate) + "Hz, " +
        juce::String(currentNumChannels) + " channels, " +
        juce::String(maxRecordingSamples) + " max samples (" +
        juce::String(maxRecordingSamples / sampleRate) + "s)");
}

void AudioCapture::reset()
{
    recording.store(false);
    recordingPosition.store(0);
    hasRecordedData = false;
    recordedAudio.clear();
    resetLevels();
}

//==============================================================================
void AudioCapture::startRecording(const juce::String& recordingId)
{
    if (recording.load())
    {
        DBG("Already recording, ignoring start request");
        return;
    }

    currentRecordingId = recordingId;
    hasRecordedData = false;

    // Clear the recording buffer
    recordingBuffer.clear();

    // Reset levels before starting
    resetLevels();
    rmsSums[0] = 0.0f;
    rmsSums[1] = 0.0f;
    rmsSampleCount = 0;

    // Reset position and start recording (order matters for thread safety)
    recordingPosition.store(0);
    recording.store(true);

    DBG("Started audio capture: " + recordingId);
}

juce::AudioBuffer<float> AudioCapture::stopRecording()
{
    if (!recording.load())
    {
        DBG("Not recording, returning empty buffer");
        return juce::AudioBuffer<float>();
    }

    // Stop recording first (audio thread will stop writing)
    recording.store(false);

    // Get the final position
    int finalPosition = recordingPosition.load();

    // Copy recorded data to return buffer
    if (finalPosition > 0)
    {
        recordedAudio.setSize(currentNumChannels, finalPosition, false, true, false);

        for (int channel = 0; channel < currentNumChannels; ++channel)
        {
            recordedAudio.copyFrom(channel, 0, recordingBuffer, channel, 0, finalPosition);
        }

        hasRecordedData = true;

        DBG("Stopped recording: " + juce::String(finalPosition) + " samples, " +
            juce::String(finalPosition / currentSampleRate) + " seconds");
    }

    auto result = recordedAudio;

    // Reset for next recording
    recordingPosition.store(0);
    currentRecordingId = "";

    return result;
}

//==============================================================================
void AudioCapture::captureAudio(const juce::AudioBuffer<float>& buffer)
{
    // Fast exit if not recording (atomic read)
    if (!recording.load(std::memory_order_relaxed))
        return;

    int currentPos = recordingPosition.load(std::memory_order_relaxed);

    // Check if we've reached the max
    if (currentPos >= maxRecordingSamples)
        return;

    int numSamples = buffer.getNumSamples();
    int numChannels = juce::jmin(buffer.getNumChannels(), currentNumChannels);

    // Calculate how many samples we can write
    int samplesToWrite = juce::jmin(numSamples, maxRecordingSamples - currentPos);

    if (samplesToWrite > 0)
    {
        // Copy audio data to recording buffer (lock-free write)
        for (int channel = 0; channel < numChannels; ++channel)
        {
            recordingBuffer.copyFrom(channel, currentPos, buffer, channel, 0, samplesToWrite);
        }

        // Update position atomically
        recordingPosition.store(currentPos + samplesToWrite, std::memory_order_relaxed);
    }

    // Update level meters (always, even when buffer is full)
    updateLevels(buffer, numSamples);
}

//==============================================================================
void AudioCapture::updateLevels(const juce::AudioBuffer<float>& buffer, int numSamples)
{
    int numChannels = juce::jmin(buffer.getNumChannels(), MaxChannels);

    for (int channel = 0; channel < numChannels; ++channel)
    {
        const float* data = buffer.getReadPointer(channel);

        // Calculate peak for this buffer
        float bufferPeak = 0.0f;
        float sumSquares = 0.0f;

        for (int i = 0; i < numSamples; ++i)
        {
            float sample = std::abs(data[i]);
            bufferPeak = juce::jmax(bufferPeak, sample);
            sumSquares += data[i] * data[i];
        }

        // Update peak with decay (peak hold with fast attack, slow decay)
        float currentPeak = peakLevels[channel].load(std::memory_order_relaxed);
        if (bufferPeak > currentPeak)
        {
            peakLevels[channel].store(bufferPeak, std::memory_order_relaxed);
        }
        else
        {
            // Decay: ~300ms to reach 10% at 44.1kHz with 512 sample buffers
            float decay = 0.95f;
            peakLevels[channel].store(currentPeak * decay, std::memory_order_relaxed);
        }

        // Accumulate RMS
        rmsSums[channel] += sumSquares;
        rmsSampleCount += numSamples;

        // Update RMS when we have enough samples
        if (rmsSampleCount >= RMSWindowSamples)
        {
            float rms = std::sqrt(rmsSums[channel] / static_cast<float>(rmsSampleCount));
            rmsLevels[channel].store(rms, std::memory_order_relaxed);

            // Reset accumulators
            rmsSums[channel] = 0.0f;
            if (channel == numChannels - 1)
            {
                rmsSampleCount = 0;
            }
        }
    }
}

//==============================================================================
double AudioCapture::getRecordingLengthSeconds() const
{
    if (currentSampleRate <= 0)
        return 0.0;

    return recordingPosition.load() / currentSampleRate;
}

double AudioCapture::getMaxRecordingLengthSeconds() const
{
    if (currentSampleRate <= 0)
        return 60.0;

    return maxRecordingSamples / currentSampleRate;
}

float AudioCapture::getRecordingProgress() const
{
    if (maxRecordingSamples <= 0)
        return 0.0f;

    return static_cast<float>(recordingPosition.load()) / static_cast<float>(maxRecordingSamples);
}

bool AudioCapture::isBufferFull() const
{
    return recordingPosition.load() >= maxRecordingSamples;
}

//==============================================================================
float AudioCapture::getPeakLevel(int channel) const
{
    if (channel < 0 || channel >= MaxChannels)
        return 0.0f;

    return peakLevels[channel].load(std::memory_order_relaxed);
}

float AudioCapture::getRMSLevel(int channel) const
{
    if (channel < 0 || channel >= MaxChannels)
        return 0.0f;

    return rmsLevels[channel].load(std::memory_order_relaxed);
}

void AudioCapture::resetLevels()
{
    for (int i = 0; i < MaxChannels; ++i)
    {
        peakLevels[i].store(0.0f, std::memory_order_relaxed);
        rmsLevels[i].store(0.0f, std::memory_order_relaxed);
    }
}

juce::String AudioCapture::generateWaveformSVG(const juce::AudioBuffer<float>& buffer, int width, int height)
{
    if (buffer.getNumSamples() == 0 || buffer.getNumChannels() == 0)
        return "";
    
    // Generate SVG waveform visualization
    juce::String svg = "<svg width=\"" + juce::String(width) + "\" height=\"" + juce::String(height) + "\" xmlns=\"http://www.w3.org/2000/svg\">";
    svg += "<rect width=\"100%\" height=\"100%\" fill=\"#1a1a1e\"/>";
    
    // Calculate samples per pixel
    int numSamples = buffer.getNumSamples();
    float samplesPerPixel = static_cast<float>(numSamples) / width;
    
    // Generate path for waveform
    juce::String pathData = "M";
    
    for (int x = 0; x < width; ++x)
    {
        int sampleIndex = static_cast<int>(x * samplesPerPixel);
        
        if (sampleIndex < numSamples)
        {
            // Get peak amplitude for this pixel (mono or average of channels)
            float peak = 0.0f;
            for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            {
                peak += std::abs(buffer.getSample(channel, sampleIndex));
            }
            peak /= buffer.getNumChannels();
            
            // Convert to SVG coordinates
            int y = static_cast<int>((1.0f - peak) * height * 0.5f);
            y = juce::jlimit(0, height, y);
            
            if (x == 0)
                pathData += juce::String(x) + "," + juce::String(height / 2);
            else
                pathData += " L" + juce::String(x) + "," + juce::String(y);
        }
    }
    
    svg += "<path d=\"" + pathData + "\" stroke=\"#00d4ff\" stroke-width=\"1\" fill=\"none\"/>";
    svg += "</svg>";
    
    return svg;
}

//==============================================================================
void AudioCapture::initializeBuffers()
{
    recordingBuffer.setSize(currentNumChannels, maxRecordingSamples, false, true, false);
    recordingBuffer.clear();
    recordingPosition = 0;
}