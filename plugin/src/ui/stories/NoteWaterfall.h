#pragma once

#include <JuceHeader.h>
#include <vector>

// ==============================================================================
/**
 * NoteWaterfall visualizes MIDI data in a waterfall/falling notes style
 * (7.5.5.2.1)
 *
 * Features:
 * - Notes fall from top as they play
 * - Color by velocity (brighter = louder)
 * - Minimalist, visually appealing design
 * - Smooth animations during playback
 * - Active notes glow at the bottom
 *
 * MIDI data format expected (from MIDICapture::getMIDIDataAsJSON):
 * {
 *   "events": [
 *     {"time": 0.0, "type": "note_on", "note": 60, "velocity": 100, "channel":
 * 0},
 *     {"time": 0.5, "type": "note_off", "note": 60, "channel": 0}
 *   ],
 *   "total_time": 30.5,
 *   "tempo": 128
 * }
 */
class NoteWaterfall : public juce::Component, public juce::Timer {
public:
  NoteWaterfall();
  ~NoteWaterfall() override;

  // ==============================================================================
  void paint(juce::Graphics &) override;
  void resized() override;

  // ==============================================================================
  // Timer for animation
  void timerCallback() override;

  // ==============================================================================
  // MIDI data management

  // Set MIDI data from JSON (from MIDICapture::getMIDIDataAsJSON)
  void setMIDIData(const juce::var &midiData);

  // Clear all MIDI data
  void clearMIDIData();

  // Set current playback position (for animation)
  void setPlaybackPosition(double positionSeconds);

  // Callback for seeking to a time position
  std::function<void(double timeSeconds)> onSeekToTime;

  // ==============================================================================
  // Display options

  // Set visible note range (default C1-C7 = 24-96)
  void setNoteRange(int lowNote, int highNote);

  // Set lookahead time (how far ahead notes appear)
  void setLookaheadTime(double seconds) {
    lookaheadTime = seconds;
    repaint();
  }

  // Enable/disable velocity-based coloring
  void setShowVelocity(bool show) {
    showVelocity = show;
    repaint();
  }

  // Enable/disable channel coloring
  void setShowChannels(bool show) {
    showChannels = show;
    repaint();
  }

private:
  // ==============================================================================
  // Internal note representation (derived from MIDI events)
  struct Note {
    double startTime;
    double endTime;
    int noteNumber;
    int velocity;
    int channel;

    // Check if note is playing at given time
    bool isPlayingAt(double time) const {
      return time >= startTime && time < endTime;
    }

    // Check if note is visible (within lookahead window)
    bool isVisibleAt(double time, double lookahead) const {
      return startTime <= time + lookahead && endTime >= time;
    }
  };

  std::vector<Note> notes;
  double totalDuration = 0.0;
  double tempo = 120.0;

  // Display state
  double playbackPosition = 0.0;
  int lowNoteNumber = 24;     // C1
  int highNoteNumber = 96;    // C7
  double lookaheadTime = 3.0; // Show notes 3 seconds ahead
  bool showVelocity = true;
  bool showChannels = false;

  // Animation state
  float pulsePhase = 0.0f;

  // ==============================================================================
  // Drawing helpers
  void drawBackground(juce::Graphics &g);
  void drawNotes(juce::Graphics &g);
  void drawActiveNotesGlow(juce::Graphics &g);
  void drawKeyIndicators(juce::Graphics &g);

  // Utility
  bool isBlackKey(int noteNumber) const;
  juce::String getNoteName(int noteNumber) const;
  float noteToX(int noteNumber) const;
  float timeToY(double time) const;
  juce::Colour getNoteColor(const Note &note) const;
  juce::Colour getChannelColor(int channel) const;

  // Parse MIDI events into notes
  void parseMIDIEvents(const juce::var &events);

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NoteWaterfall)
};
