#include "WaveformGenerator.h"
#include "Async.h"
#include "Log.h"
#include <JuceHeader.h>

// ==============================================================================
juce::Path WaveformGenerator::generateWaveformPath(const juce::AudioBuffer<float> &buffer,
                                                   juce::Rectangle<int> bounds) {
  juce::Path path;

  if (buffer.getNumSamples() == 0)
    return path;

  int numSamples = buffer.getNumSamples();
  int width = bounds.getWidth();
  float height = static_cast<float>(bounds.getHeight());
  float centerY = static_cast<float>(bounds.getCentreY());

  path.startNewSubPath(static_cast<float>(bounds.getX()), centerY);

  for (int x = 0; x < width; ++x) {
    int startSample = x * numSamples / width;
    int endSample = juce::jmin((x + 1) * numSamples / width, numSamples);

    float peak = 0.0f;
    for (int i = startSample; i < endSample; ++i) {
      for (int ch = 0; ch < buffer.getNumChannels(); ++ch) {
        peak = juce::jmax(peak, std::abs(buffer.getSample(ch, i)));
      }
    }

    float y = centerY - (peak * height * 0.5f);
    path.lineTo(static_cast<float>(bounds.getX() + x), y);
  }

  return path;
}

void WaveformGenerator::generateWaveformPathFromUrl(const juce::String &audioUrl, juce::Rectangle<int> bounds,
                                                    std::function<void(juce::Path)> callback) {
  if (audioUrl.isEmpty() || !callback) {
    if (callback)
      callback(juce::Path());
    return;
  }

  // Download and decode audio on background thread
  Async::run<juce::AudioBuffer<float>>(
      [audioUrl]() -> juce::AudioBuffer<float> {
        Log::debug("WaveformGenerator: Downloading audio from: " + audioUrl);

        juce::URL url(audioUrl);
        auto inputStream = url.createInputStream(juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                                                     .withConnectionTimeoutMs(10000)
                                                     .withNumRedirectsToFollow(5));

        if (inputStream == nullptr) {
          Log::error("WaveformGenerator: Failed to create input stream");
          return juce::AudioBuffer<float>();
        }

        juce::MemoryBlock audioData;
        inputStream->readIntoMemoryBlock(audioData);

        if (audioData.isEmpty()) {
          Log::error("WaveformGenerator: Empty audio data");
          return juce::AudioBuffer<float>();
        }

        // Try to decode as audio file
        juce::AudioFormatManager formatManager;
        formatManager.registerBasicFormats();

        auto *reader = formatManager.createReaderFor(std::make_unique<juce::MemoryInputStream>(audioData, false));

        if (reader == nullptr) {
          Log::error("WaveformGenerator: Failed to create audio reader");
          return juce::AudioBuffer<float>();
        }

        // Read audio into buffer
        juce::AudioBuffer<float> buffer(static_cast<int>(reader->numChannels),
                                        static_cast<int>(reader->lengthInSamples));

        if (reader->read(&buffer, 0, static_cast<int>(reader->lengthInSamples), 0, true, true)) {
          Log::debug("WaveformGenerator: Successfully decoded audio - " + juce::String(buffer.getNumSamples()) +
                     " samples, " + juce::String(buffer.getNumChannels()) + " channels");
          return buffer;
        }

        Log::error("WaveformGenerator: Failed to read audio data");
        return juce::AudioBuffer<float>();
      },
      [bounds, callback](const juce::AudioBuffer<float> &buffer) {
        juce::MessageManager::callAsync([buffer, bounds, callback]() {
          if (buffer.getNumSamples() > 0) {
            auto path = generateWaveformPath(buffer, bounds);
            callback(path);
          } else {
            Log::warn("WaveformGenerator: Empty buffer, returning empty path");
            callback(juce::Path());
          }
        });
      });
}
