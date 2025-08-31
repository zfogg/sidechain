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
    currentNumChannels = numChannels;
    
    // Initialize buffers for 30 seconds of audio maximum
    maxRecordingSamples = static_cast<int>(sampleRate * 30.0);
    
    initializeBuffers();
    
    DBG("AudioCapture prepared: " + juce::String(sampleRate) + "Hz, " + 
        juce::String(numChannels) + " channels, " + 
        juce::String(maxRecordingSamples) + " max samples");
}

void AudioCapture::reset()
{
    recording = false;
    recordingPosition = 0;
    hasRecordedData = false;
    recordedAudio.clear();
}

//==============================================================================
void AudioCapture::startRecording(const juce::String& recordingId)
{
    if (recording)
    {
        DBG("Already recording, ignoring start request");
        return;
    }
    
    currentRecordingId = recordingId;
    recording = true;
    recordingPosition = 0;
    hasRecordedData = false;
    
    // Clear the recording buffer
    recordingBuffer.clear();
    
    DBG("Started audio capture: " + recordingId);
}

juce::AudioBuffer<float> AudioCapture::stopRecording()
{
    if (!recording)
    {
        DBG("Not recording, returning empty buffer");
        return juce::AudioBuffer<float>();
    }
    
    recording = false;
    
    // Copy recorded data to return buffer
    if (recordingPosition > 0)
    {
        recordedAudio.setSize(currentNumChannels, recordingPosition, false, true, false);
        
        for (int channel = 0; channel < currentNumChannels; ++channel)
        {
            recordedAudio.copyFrom(channel, 0, recordingBuffer, channel, 0, recordingPosition);
        }
        
        hasRecordedData = true;
        
        DBG("Stopped recording: " + juce::String(recordingPosition) + " samples, " + 
            juce::String(recordingPosition / currentSampleRate) + " seconds");
    }
    
    auto result = recordedAudio;
    
    // Reset for next recording
    recordingPosition = 0;
    currentRecordingId = "";
    
    return result;
}

//==============================================================================
void AudioCapture::captureAudio(const juce::AudioBuffer<float>& buffer)
{
    if (!recording || recordingPosition >= maxRecordingSamples)
        return;
        
    int numSamples = buffer.getNumSamples();
    int numChannels = juce::jmin(buffer.getNumChannels(), currentNumChannels);
    
    // Check if we have space in the recording buffer
    int samplesToWrite = juce::jmin(numSamples, maxRecordingSamples - recordingPosition);
    
    if (samplesToWrite > 0)
    {
        // Copy audio data to recording buffer
        for (int channel = 0; channel < numChannels; ++channel)
        {
            recordingBuffer.copyFrom(channel, recordingPosition, buffer, channel, 0, samplesToWrite);
        }
        
        recordingPosition += samplesToWrite;
    }
    
    // Auto-stop if we've reached maximum length
    if (recordingPosition >= maxRecordingSamples)
    {
        DBG("Recording reached maximum length, auto-stopping");
        // Note: We don't call stopRecording() here to avoid thread issues
        // The UI should check getRecordingLengthSeconds() and stop manually
    }
}

//==============================================================================
double AudioCapture::getRecordingLengthSeconds() const
{
    if (currentSampleRate <= 0)
        return 0.0;
        
    return recordingPosition / currentSampleRate;
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