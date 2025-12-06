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

    /** MIDI event structure for captured MIDI data */
    struct MIDIEvent
    {
        double time;       ///< Relative time in seconds from recording start
        juce::String type; ///< Event type ("note_on" or "note_off")
        int note;         ///< MIDI note number (0-127)
        int velocity;      ///< Note velocity (0-127)
        int channel;      ///< MIDI channel (0-15)
    };

    //==============================================================================
    /** Constructor */
    MIDICapture();

    /** Destructor */
    ~MIDICapture();

    //==============================================================================
    // Configuration - call from prepareToPlay() or message thread

    /** Prepare MIDI capture for recording
     *  @param sampleRate The sample rate of the audio system
     *  @param samplesPerBlock The block size used by the audio system
     */
    void prepare(double sampleRate, int samplesPerBlock);

    /** Reset all capture state and clear recorded events */
    void reset();

    //==============================================================================
    // Recording control - call from MESSAGE THREAD only

    /** Start capturing MIDI events
     *  Resets previous capture data and begins recording new events
     */
    void startCapture();

    /** Stop capturing and return all captured MIDI events
     *  @return Vector of all captured MIDI events
     */
    std::vector<MIDIEvent> stopCapture();

    /** Check if MIDI capture is currently active
     *  @return true if capturing, false otherwise
     */
    bool isCapturing() const { return capturing.load(); }

    //==============================================================================
    // MIDI capture - call from AUDIO THREAD (processBlock) only
    // MUST be lock-free and allocation-free

    /**
     * Capture MIDI events from the DAW's processBlock callback.
     *
     * @warning CRITICAL: This function MUST be called from the AUDIO THREAD only.
     *          Calling from any other thread will cause audio dropouts and crashes.
     *
     * @warning MUST be lock-free and allocation-free. This function uses atomic
     *          operations and lock-free data structures to ensure real-time
     *          audio performance. Any blocking operations or memory allocations
     *          will cause audio glitches.
     *
     * @param midiMessages MIDI message buffer from processBlock
     * @param numSamples Number of audio samples in the current block
     * @param sampleRate Current sample rate (for timing calculations)
     *
     * @note This function is designed to be called every audio block during
     *       MIDI capture. It extracts note_on and note_off events and stores
     *       them with precise timing relative to the recording start.
     *
     * @note MIDI events are timestamped using the current sample position,
     *       allowing accurate synchronization with the audio timeline.
     *
     * @see startCapture() To begin MIDI capture (call from message thread)
     * @see stopCapture() To end capture and get all events (call from message thread)
     * @see PluginProcessor::processBlock() For typical usage in audio callback
     */
    void captureMIDI(const juce::MidiBuffer& midiMessages, int numSamples, double sampleRate);

    //==============================================================================
    // MIDI data export - thread-safe

    /** Get all captured MIDI events as JSON
     *  @return JSON var containing array of MIDI events
     */
    juce::var getMIDIDataAsJSON() const;

    /** Get total recording time in seconds
     *  @return Total time of the captured MIDI sequence
     */
    double getTotalTime() const { return totalTimeSeconds.load(); }

    //==============================================================================
    // MIDI data processing (7.5.2.2)
    // Call after stopCapture() to clean up the data before upload

    /** Normalize MIDI timing to relative time from recording start
     *  Converts timestamps to relative time (0.0 = start of recording),
     *  rounds to millisecond precision, and handles tempo changes if present
     *  @param events Vector of MIDI events to normalize
     *  @return Vector of events with normalized timing
     */
    static std::vector<MIDIEvent> normalizeTiming(const std::vector<MIDIEvent>& events);

    /** Validate MIDI data for consistency
     *  Ensures note_on has matching note_off, removes duplicate events,
     *  and filters out invalid notes (outside 0-127)
     *  @param events Vector of MIDI events to validate
     *  @return Vector of validated events
     */
    static std::vector<MIDIEvent> validateEvents(const std::vector<MIDIEvent>& events);

    /** Get normalized and validated MIDI data as JSON
     *  Convenience method that applies both normalization and validation
     *  @return JSON var containing normalized and validated MIDI events
     */
    juce::var getNormalizedMIDIDataAsJSON() const;

    /** Set tempo from DAW for proper timing normalization
     *  @param bpm Tempo in beats per minute
     */
    void setTempo(double bpm) { currentTempo.store(bpm); }

    /** Get current tempo
     *  @return Tempo in beats per minute
     */
    double getTempo() const { return currentTempo.load(); }

    /** Set time signature from DAW
     *  @param numerator Time signature numerator (e.g., 4 for 4/4)
     *  @param denominator Time signature denominator (e.g., 4 for 4/4)
     */
    void setTimeSignature(int numerator, int denominator);

    /** Get current time signature
     *  @return Pair of (numerator, denominator)
     */
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


