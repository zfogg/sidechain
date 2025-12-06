#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <vector>

//==============================================================================
/**
 * MIDICapture handles capturing MIDI events from the DAW during recording
 *
 * Thread Safety:
 * - captureMIDI() is called from the AUDIO THREAD (processBlock)
 * - All other methods are called from the MESSAGE THREAD
 * - Uses std::atomic for thread-safe state sharing
 *
 * Features:
 * - Lock-free MIDI event capture from processBlock
 * - Captures note_on/note_off events with precise timing
 * - Stores velocity, channel, and note number
 * - Syncs MIDI events with audio timeline
 */
class MIDICapture
{
public:
    //==============================================================================
    // MIDI Event structure (matches backend MIDIEvent)
    struct MIDIEvent
    {
        double time;      // Relative time in seconds from recording start
        juce::String type; // "note_on" or "note_off"
        int note;         // MIDI note number (0-127)
        int velocity;     // Note velocity (0-127)
        int channel;      // MIDI channel (0-15)
    };

    //==============================================================================
    MIDICapture();
    ~MIDICapture();

    //==============================================================================
    // Configuration - call from prepareToPlay() or message thread
    void prepare(double sampleRate, int samplesPerBlock);
    void reset();

    //==============================================================================
    // Recording control - call from MESSAGE THREAD only
    void startCapture();
    std::vector<MIDIEvent> stopCapture();
    bool isCapturing() const { return capturing.load(); }

    //==============================================================================
    // MIDI capture - call from AUDIO THREAD (processBlock) only
    // MUST be lock-free and allocation-free
    void captureMIDI(const juce::MidiBuffer& midiMessages, int numSamples, double sampleRate);

    //==============================================================================
    // MIDI data export - thread-safe
    juce::var getMIDIDataAsJSON() const;
    double getTotalTime() const { return totalTimeSeconds.load(); }

    //==============================================================================
    // MIDI data processing (7.5.2.2)
    // Call after stopCapture() to clean up the data before upload

    // Normalize MIDI timing (7.5.2.2.1)
    // - Converts timestamps to relative time (0.0 = start of recording)
    // - Rounds to millisecond precision
    // - Handles tempo changes if present
    static std::vector<MIDIEvent> normalizeTiming(const std::vector<MIDIEvent>& events);

    // Validate MIDI data (7.5.2.2.2)
    // - Ensures note_on has matching note_off
    // - Removes duplicate events
    // - Filters out invalid notes (outside 0-127)
    // Returns validated events
    static std::vector<MIDIEvent> validateEvents(const std::vector<MIDIEvent>& events);

    // Get normalized and validated MIDI data as JSON
    // Convenience method that applies both normalization and validation
    juce::var getNormalizedMIDIDataAsJSON() const;

    // Set tempo from DAW (for proper timing normalization)
    void setTempo(double bpm) { currentTempo.store(bpm); }
    double getTempo() const { return currentTempo.load(); }

    // Set time signature from DAW
    void setTimeSignature(int numerator, int denominator);
    std::pair<int, int> getTimeSignature() const;

private:
    //==============================================================================
    // Thread-safe state
    std::atomic<bool> capturing { false };
    std::atomic<double> totalTimeSeconds { 0.0 };
    std::atomic<int> currentSamplePosition { 0 };

    // MIDI events (protected by mutex for message thread access)
    mutable juce::CriticalSection eventsLock;
    std::vector<MIDIEvent> midiEvents;

    // Audio settings
    double currentSampleRate = 44100.0;
    int currentBlockSize = 512;

    // Tempo and time signature (from DAW)
    std::atomic<double> currentTempo { 120.0 };
    std::atomic<int> timeSignatureNumerator { 4 };
    std::atomic<int> timeSignatureDenominator { 4 };

    // Helper methods
    void addEvent(const MIDIEvent& event);
    double samplePositionToTime(int samplePosition) const;
};


