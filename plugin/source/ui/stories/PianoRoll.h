#pragma once

#include <JuceHeader.h>
#include <vector>

//==============================================================================
/**
 * PianoRoll visualizes MIDI data in a piano roll format (7.5.5.1.1)
 *
 * Features:
 * - Vertical piano keys (configurable range)
 * - Horizontal timeline synced with audio playback
 * - Note rectangles with velocity-based coloring
 * - Playback position indicator
 * - Animated note highlighting during playback
 *
 * MIDI data format expected (from MIDICapture::getMIDIDataAsJSON):
 * {
 *   "events": [
 *     {"time": 0.0, "type": "note_on", "note": 60, "velocity": 100, "channel": 0},
 *     {"time": 0.5, "type": "note_off", "note": 60, "channel": 0}
 *   ],
 *   "total_time": 30.5,
 *   "tempo": 128
 * }
 */
class PianoRoll : public juce::Component,
                            public juce::Timer
{
public:
    PianoRoll();
    ~PianoRoll() override;

    //==============================================================================
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseMove(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
    void mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override;

    //==============================================================================
    // Timer for animation
    void timerCallback() override;

    //==============================================================================
    // MIDI data management

    // Set MIDI data from JSON (from MIDICapture::getMIDIDataAsJSON)
    void setMIDIData(const juce::var& midiData);

    // Clear all MIDI data
    void clearMIDIData();

    // Set current playback position (for note highlighting)
    void setPlaybackPosition(double positionSeconds);

    // Callback for seeking to a time position
    std::function<void(double timeSeconds)> onSeekToTime;

    //==============================================================================
    // Display options

    // Set visible note range (default C1-C7 = 24-96)
    void setNoteRange(int lowNote, int highNote);

    // Enable/disable velocity-based coloring
    void setShowVelocity(bool show) { showVelocity = show; repaint(); }

    // Enable/disable channel coloring
    void setShowChannels(bool show) { showChannels = show; repaint(); }

    // Set piano key width
    void setPianoKeyWidth(int width) { pianoKeyWidth = width; resized(); }

private:
    //==============================================================================
    // Internal note representation (derived from MIDI events)
    struct Note
    {
        double startTime;
        double endTime;
        int noteNumber;
        int velocity;
        int channel;

        // Check if note is playing at given time
        bool isPlayingAt(double time) const
        {
            return time >= startTime && time < endTime;
        }
    };

    std::vector<Note> notes;
    double totalDuration = 0.0;
    double tempo = 120.0;

    // Display state
    double playbackPosition = 0.0;
    int lowNoteNumber = 24;   // C1
    int highNoteNumber = 96;  // C7
    int pianoKeyWidth = 40;
    bool showVelocity = true;
    bool showChannels = false;

    // UI areas
    juce::Rectangle<int> pianoKeyArea;
    juce::Rectangle<int> noteGridArea;

    // Animation state
    float pulsePhase = 0.0f;

    // Interaction state
    int hoveredNoteIndex = -1;
    juce::Point<int> lastMousePosition;
    double timelineScrollOffset = 0.0;  // Horizontal scroll offset in seconds
    double zoomLevel = 1.0;  // Zoom level (1.0 = normal, >1.0 = zoomed in)

    //==============================================================================
    // Drawing helpers
    void drawPianoKeys(juce::Graphics& g);
    void drawNoteGrid(juce::Graphics& g);
    void drawNotes(juce::Graphics& g);
    void drawPlayhead(juce::Graphics& g);
    void drawGridLines(juce::Graphics& g);

    // Utility
    bool isBlackKey(int noteNumber) const;
    juce::String getNoteName(int noteNumber) const;
    float noteToY(int noteNumber) const;
    float timeToX(double time) const;
    double xToTime(float x) const;
    juce::Colour getNoteColor(const Note& note) const;
    juce::Colour getChannelColor(int channel) const;

    // Find note at position
    int findNoteAt(juce::Point<int> position) const;

    // Parse MIDI events into notes
    void parseMIDIEvents(const juce::var& events);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PianoRoll)
};
