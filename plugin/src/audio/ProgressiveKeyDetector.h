#pragma once

#include <JuceHeader.h>
#include "KeyDetector.h"

//==============================================================================
/**
 * ProgressiveKeyDetector provides progressive/streaming key detection for audio data.
 *
 * Uses libkeyfinder's progressive estimation API to analyze audio in chunks,
 * allowing for real-time key detection during recording or streaming.
 *
 * Usage:
 *   1. Call start() with the sample rate
 *   2. Repeatedly call addAudioChunk() with audio buffers
 *   3. Optionally call getCurrentKey() to get progressive estimates
 *   4. Call finalize() when done adding audio
 *   5. Call getFinalKey() to get the final result
 *   6. Call reset() to start a new analysis
 *
 * Progressive Estimation Process:
 *   1. Audio chunks are processed incrementally using progressiveChromagram()
 *   2. Chromagram is built up progressively as more audio is added
 *   3. Key estimates can be obtained at any time using keyOfChromagram()
 *   4. finalChromagram() is called to finalize when done
 *
 * This is more efficient than KeyDetector for:
 *   - Real-time key detection during recording
 *   - Streaming audio analysis
 *   - Large files processed in chunks
 */
class ProgressiveKeyDetector
{
public:
    //==========================================================================
    ProgressiveKeyDetector();
    ~ProgressiveKeyDetector();

    //==========================================================================
    /**
     * Start a new progressive key detection session.
     *
     * @param sampleRate Sample rate of the audio (must be consistent for all chunks)
     * @return true if initialization successful, false if libkeyfinder unavailable
     */
    bool start(double sampleRate);

    /**
     * Add a chunk of audio to the progressive analysis.
     *
     * @param buffer Audio buffer (will be mixed to mono internally)
     * @param numChannels Number of channels in the buffer
     * @return true if successful, false on error
     */
    bool addAudioChunk(const juce::AudioBuffer<float>& buffer, int numChannels);

    /**
     * Get the current key estimate without finalizing.
     * Can be called at any time during progressive analysis.
     *
     * @return Current key estimate (may be invalid if not enough audio processed)
     */
    KeyDetector::Key getCurrentKey() const;

    /**
     * Finalize the progressive analysis.
     * Must be called after all audio chunks have been added.
     *
     * @return true if successful, false on error
     */
    bool finalize();

    /**
     * Get the final key after finalization.
     * Must call finalize() first.
     *
     * @return Final detected key
     */
    KeyDetector::Key getFinalKey() const;

    /**
     * Reset the detector for a new analysis session.
     * Clears all internal state.
     */
    void reset();

    /**
     * Check if a detection session is active.
     *
     * @return true if started and not finalized/reset
     */
    bool isActive() const { return active; }

    /**
     * Check if the analysis has been finalized.
     *
     * @return true if finalize() has been called
     */
    bool isFinalized() const { return finalized; }

    /**
     * Get the total number of samples processed so far.
     *
     * @return Number of samples processed
     */
    int getSamplesProcessed() const { return samplesProcessed; }

    /**
     * Check if progressive key detection is available.
     * (libkeyfinder compiled in and initialized)
     */
    static bool isAvailable();

private:
    class Impl;
    std::unique_ptr<Impl> impl;

    bool active = false;
    bool finalized = false;
    double sampleRate = 0.0;
    int samplesProcessed = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ProgressiveKeyDetector)
};
