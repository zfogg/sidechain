#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <vector>
#include <functional>
#include <set>

//==============================================================================
/**
 * ChordSequenceDetector detects chord sequences for unlocking easter eggs
 *
 * Features:
 * - Real-time chord detection from MIDI input
 * - Pattern matching for unlock sequences
 * - Callback system for triggering unlocks
 * - Support for multiple unlock sequences
 */
class ChordSequenceDetector
{
public:
    //==============================================================================
    /** Chord types */
    enum class ChordType
    {
        Unknown,
        Major,
        Minor,
        Diminished,
        Augmented,
        Sus2,
        Sus4,
        Major7,
        Minor7,
        Dominant7
    };

    /** Detected chord structure */
    struct Chord
    {
        int rootNote;           // Root note (0-11, C=0)
        ChordType type;
        std::set<int> notes;    // All notes in the chord (pitch classes 0-11)
        double timestamp;       // When chord was detected

        bool operator==(const Chord& other) const
        {
            return rootNote == other.rootNote && type == other.type;
        }

        juce::String toString() const
        {
            static const char* noteNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
            juce::String name = noteNames[rootNote % 12];

            switch (type)
            {
                case ChordType::Major: break; // No suffix for major
                case ChordType::Minor: name += "m"; break;
                case ChordType::Diminished: name += "dim"; break;
                case ChordType::Augmented: name += "aug"; break;
                case ChordType::Sus2: name += "sus2"; break;
                case ChordType::Sus4: name += "sus4"; break;
                case ChordType::Major7: name += "maj7"; break;
                case ChordType::Minor7: name += "m7"; break;
                case ChordType::Dominant7: name += "7"; break;
                default: name += "?"; break;
            }
            return name;
        }
    };

    /** Unlock sequence definition */
    struct UnlockSequence
    {
        juce::String name;                      // Sequence name (e.g., "basic_synth")
        std::vector<Chord> chords;              // Required chord sequence
        std::function<void()> onUnlock;         // Callback when unlocked
        bool unlocked = false;                  // Whether already unlocked
    };

    //==============================================================================
    /** Constructor */
    ChordSequenceDetector();

    /** Destructor */
    ~ChordSequenceDetector() = default;

    //==============================================================================
    // MIDI Processing - call from AUDIO THREAD

    /** Process MIDI messages to detect chords
     *  @param midiMessages MIDI buffer from processBlock
     *  @param sampleRate Current sample rate
     */
    void processMIDI(const juce::MidiBuffer& midiMessages, double sampleRate);

    //==============================================================================
    // Configuration - call from MESSAGE THREAD

    /** Add an unlock sequence to detect
     *  @param sequence The unlock sequence to add
     */
    void addUnlockSequence(UnlockSequence sequence);

    /** Clear all unlock sequences */
    void clearUnlockSequences();

    /** Reset detection state (clear current chord buffer) */
    void reset();

    /** Set whether detection is enabled
     *  @param enabled True to enable chord detection
     */
    void setEnabled(bool enabled) { detecting.store(enabled); }

    /** Check if detection is enabled */
    bool isEnabled() const { return detecting.load(); }

    //==============================================================================
    // State queries - thread-safe

    /** Get the currently detected chord (or empty if none) */
    Chord getCurrentChord() const;

    /** Get recent chord history */
    std::vector<Chord> getChordHistory() const;

    /** Check if a specific sequence has been unlocked
     *  @param sequenceName Name of the sequence to check
     *  @return True if unlocked
     */
    bool isSequenceUnlocked(const juce::String& sequenceName) const;

    /** Get all unlocked sequence names */
    std::vector<juce::String> getUnlockedSequences() const;

    //==============================================================================
    // Predefined unlock sequences

    /** Create the basic synth unlock sequence (C-E-G-C arpeggio played as chord) */
    static UnlockSequence createBasicSynthSequence(std::function<void()> onUnlock);

    /** Create the advanced synth unlock sequence (Am-F-C-G progression) */
    static UnlockSequence createAdvancedSynthSequence(std::function<void()> onUnlock);

    /** Create secret sequence (Dm-G-C-Am - the "I-V-vi-IV" in reverse) */
    static UnlockSequence createSecretSequence(std::function<void()> onUnlock);

private:
    //==============================================================================
    // Chord detection helpers

    /** Identify chord from currently held notes */
    Chord identifyChord(const std::set<int>& heldNotes);

    /** Get pitch class (0-11) from MIDI note */
    static int getPitchClass(int midiNote) { return midiNote % 12; }

    /** Check if a set of pitch classes forms a specific chord type */
    static ChordType identifyChordType(const std::set<int>& pitchClasses, int& outRoot);

    /** Check if current chord history matches a sequence */
    bool checkSequenceMatch(const UnlockSequence& sequence);

    /** Trigger unlock callback on message thread */
    void triggerUnlock(UnlockSequence& sequence);

    //==============================================================================
    // State
    std::atomic<bool> detecting { true };

    // Currently held notes (MIDI note numbers)
    std::set<int> currentlyHeldNotes;

    // Current detected chord
    mutable juce::CriticalSection chordLock;
    Chord currentChord;

    // Chord history for sequence matching
    std::vector<Chord> chordHistory;
    static constexpr size_t maxHistorySize = 10;

    // Unlock sequences
    mutable juce::CriticalSection sequenceLock;
    std::vector<UnlockSequence> unlockSequences;

    // Timing
    double lastChordTime = 0.0;
    double chordTimeout = 2.0; // Seconds before chord history resets

    // For detecting chord changes
    Chord lastDetectedChord;
};
