#pragma once

#include <JuceHeader.h>
#include <vector>

//==============================================================================
/**
 * CircularVisualization displays MIDI data in a radial/circular style
 * (7.5.5.2.2)
 *
 * Features:
 * - Notes arranged in a circle (like a clock or radar)
 * - Pitch determines radial position (center = low, edge = high)
 * - Time position shown as rotating sweep line
 * - Active notes highlighted with glow
 * - Artistic, less technical visualization
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
class CircularVisualization : public juce::Component, public juce::Timer {
public:
  CircularVisualization();
  ~CircularVisualization() override;

  //==============================================================================
  void paint(juce::Graphics &) override;
  void resized() override;

  //==============================================================================
  // Timer for animation
  void timerCallback() override;

  //==============================================================================
  // MIDI data management

  // Set MIDI data from JSON (from MIDICapture::getMIDIDataAsJSON)
  void setMIDIData(const juce::var &midiData);

  // Clear all MIDI data
  void clearMIDIData();

  // Set current playback position (for sweep line and highlighting)
  void setPlaybackPosition(double positionSeconds);

  // Callback for seeking to a time position
  std::function<void(double timeSeconds)> onSeekToTime;

  //==============================================================================
  // Display options

  // Enable/disable velocity-based sizing
  void setShowVelocity(bool show) {
    showVelocity = show;
    repaint();
  }

  // Enable/disable channel coloring
  void setShowChannels(bool show) {
    showChannels = show;
    repaint();
  }

  // Set visualization style
  enum class Style { Dots, Arcs, Particles };
  void setStyle(Style style) {
    visualStyle = style;
    repaint();
  }

private:
  //==============================================================================
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
  };

  std::vector<Note> notes;
  double totalDuration = 0.0;
  double tempo = 120.0;

  // Display state
  double playbackPosition = 0.0;
  int lowNoteNumber = 24;  // C1
  int highNoteNumber = 96; // C7
  bool showVelocity = true;
  bool showChannels = false;
  Style visualStyle = Style::Arcs;

  // Animation state
  float pulsePhase = 0.0f;

  // Geometry
  juce::Point<float> center;
  float innerRadius = 0.0f;
  float outerRadius = 0.0f;

  //==============================================================================
  // Drawing helpers
  void drawBackground(juce::Graphics &g);
  void drawRings(juce::Graphics &g);
  void drawNotes(juce::Graphics &g);
  void drawSweepLine(juce::Graphics &g);
  void drawActiveNotes(juce::Graphics &g);
  void drawCenterInfo(juce::Graphics &g);

  // Utility
  float timeToAngle(double time) const;
  float noteToRadius(int noteNumber) const;
  juce::Point<float> polarToCartesian(float angle, float radius) const;
  juce::Colour getNoteColor(const Note &note) const;
  juce::Colour getChannelColor(int channel) const;

  // Parse MIDI events into notes
  void parseMIDIEvents(const juce::var &events);

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CircularVisualization)
};
