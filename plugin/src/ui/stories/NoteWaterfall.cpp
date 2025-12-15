#include "NoteWaterfall.h"
#include "../../util/Log.h"

namespace WaterfallColors {
const juce::Colour background(0xff0d0d1a);
const juce::Colour gridLine(0xff1a1a2e);
const juce::Colour noteDefault(0xff7c4dff);
const juce::Colour noteActive(0xffb388ff);
const juce::Colour glowActive(0x807c4dff);
const juce::Colour keyIndicator(0xff2a2a3a);
const juce::Colour keyIndicatorBlack(0xff1a1a2a);
const juce::Colour textDim(0xff444444);

// Channel colors for multi-channel visualization
const juce::Colour channelColors[] = {
    juce::Colour(0xff7c4dff), // Purple
    juce::Colour(0xff00bcd4), // Cyan
    juce::Colour(0xff4caf50), // Green
    juce::Colour(0xffffc107), // Amber
    juce::Colour(0xffe91e63), // Pink
    juce::Colour(0xff2196f3), // Blue
    juce::Colour(0xffff5722), // Deep Orange
    juce::Colour(0xff9c27b0), // Purple
    juce::Colour(0xff00e676), // Light Green
    juce::Colour(0xffff9800), // Orange
};
} // namespace WaterfallColors

//==============================================================================
NoteWaterfall::NoteWaterfall() {
  // Start animation timer (60fps for smooth falling)
  startTimerHz(60);

  Log::debug("NoteWaterfall created");
}

NoteWaterfall::~NoteWaterfall() {
  stopTimer();
}

//==============================================================================
void NoteWaterfall::paint(juce::Graphics &g) {
  // Background
  drawBackground(g);

  // Draw key indicators at bottom
  drawKeyIndicators(g);

  // Draw falling notes
  drawNotes(g);

  // Draw glow for active notes
  drawActiveNotesGlow(g);
}

void NoteWaterfall::resized() {
  // No sub-components to layout
}

//==============================================================================
void NoteWaterfall::timerCallback() {
  // Animate pulse for active notes
  pulsePhase += 0.15f;
  if (pulsePhase > juce::MathConstants<float>::twoPi)
    pulsePhase -= juce::MathConstants<float>::twoPi;

  repaint();
}

//==============================================================================
void NoteWaterfall::setMIDIData(const juce::var &midiData) {
  notes.clear();

  if (!midiData.isObject()) {
    Log::warn("NoteWaterfall: Invalid MIDI data format");
    return;
  }

  // Get total duration
  totalDuration = static_cast<double>(midiData.getProperty("total_time", 0.0));
  tempo = static_cast<double>(midiData.getProperty("tempo", 120.0));

  // Parse events
  if (midiData.hasProperty("events")) {
    parseMIDIEvents(midiData["events"]);
  }

  // Auto-adjust note range based on content
  if (!notes.empty()) {
    int minNote = 127;
    int maxNote = 0;
    for (const auto &note : notes) {
      minNote = juce::jmin(minNote, note.noteNumber);
      maxNote = juce::jmax(maxNote, note.noteNumber);
    }

    // Add some padding
    lowNoteNumber = juce::jmax(0, minNote - 2);
    highNoteNumber = juce::jmin(127, maxNote + 2);

    // Ensure at least an octave range
    if (highNoteNumber - lowNoteNumber < 12) {
      lowNoteNumber = juce::jmax(0, (minNote + maxNote) / 2 - 6);
      highNoteNumber = lowNoteNumber + 12;
    }
  }

  Log::info("NoteWaterfall: Loaded " + juce::String(notes.size()) + " notes, " + juce::String(totalDuration, 2) +
            "s duration");

  repaint();
}

void NoteWaterfall::clearMIDIData() {
  notes.clear();
  totalDuration = 0.0;
  playbackPosition = 0.0;
  repaint();
}

void NoteWaterfall::setPlaybackPosition(double positionSeconds) {
  playbackPosition = positionSeconds;
  repaint();
}

void NoteWaterfall::setNoteRange(int lowNote, int highNote) {
  lowNoteNumber = juce::jlimit(0, 126, lowNote);
  highNoteNumber = juce::jlimit(lowNoteNumber + 1, 127, highNote);
  repaint();
}

//==============================================================================
void NoteWaterfall::drawBackground(juce::Graphics &g) {
  auto bounds = getLocalBounds();

  // Dark gradient background
  juce::ColourGradient gradient(WaterfallColors::background.darker(0.5f), 0, 0, WaterfallColors::background, 0,
                                static_cast<float>(bounds.getHeight()), false);
  g.setGradientFill(gradient);
  g.fillRect(bounds);

  // Subtle vertical grid lines for each note position
  int numNotes = highNoteNumber - lowNoteNumber + 1;
  float noteWidth = static_cast<float>(bounds.getWidth()) / static_cast<float>(numNotes);

  g.setColour(WaterfallColors::gridLine);
  for (int i = 0; i <= numNotes; ++i) {
    float x = static_cast<float>(i) * noteWidth;
    g.drawVerticalLine(static_cast<int>(x), 0, bounds.getBottom());
  }

  // Horizontal "catch line" near bottom where notes land
  float catchLineY = static_cast<float>(bounds.getHeight()) * 0.9f;
  g.setColour(WaterfallColors::noteDefault.withAlpha(0.3f));
  g.drawHorizontalLine(static_cast<int>(catchLineY), 0, bounds.getRight());
}

void NoteWaterfall::drawKeyIndicators(juce::Graphics &g) {
  auto bounds = getLocalBounds();
  int numNotes = highNoteNumber - lowNoteNumber + 1;
  float noteWidth = static_cast<float>(bounds.getWidth()) / static_cast<float>(numNotes);
  float indicatorHeight = static_cast<float>(bounds.getHeight()) * 0.1f;
  float indicatorY = static_cast<float>(bounds.getHeight()) - indicatorHeight;

  for (int i = 0; i < numNotes; ++i) {
    int noteNum = lowNoteNumber + i;
    float x = static_cast<float>(i) * noteWidth;

    auto keyBounds = juce::Rectangle<float>(x, indicatorY, noteWidth, indicatorHeight);

    // Different color for black/white keys
    if (isBlackKey(noteNum)) {
      g.setColour(WaterfallColors::keyIndicatorBlack);
    } else {
      g.setColour(WaterfallColors::keyIndicator);
    }
    g.fillRect(keyBounds);

    // Key border
    g.setColour(WaterfallColors::gridLine);
    g.drawRect(keyBounds, 1.0f);

    // Note name for C notes
    if (noteNum % 12 == 0) {
      g.setColour(WaterfallColors::textDim);
      g.setFont(8.0f);
      g.drawText(getNoteName(noteNum), keyBounds, juce::Justification::centred);
    }
  }
}

void NoteWaterfall::drawNotes(juce::Graphics &g) {
  auto bounds = getLocalBounds();
  int numNotes = highNoteNumber - lowNoteNumber + 1;
  float noteWidth = static_cast<float>(bounds.getWidth()) / static_cast<float>(numNotes);

  // Waterfall area (above key indicators)
  float waterfallHeight = static_cast<float>(bounds.getHeight()) * 0.9f;
  float catchLineY = waterfallHeight;

  for (const auto &note : notes) {
    // Check if note is visible (within lookahead window or currently playing)
    if (!note.isVisibleAt(playbackPosition, lookaheadTime))
      continue;

    // Check if note is in visible range
    if (note.noteNumber < lowNoteNumber || note.noteNumber > highNoteNumber)
      continue;

    // Calculate horizontal position
    float x = noteToX(note.noteNumber);

    // Calculate vertical positions (notes fall from top to catch line)
    // timeToY returns 0 at top (future) and catchLineY at playback position
    float topY = timeToY(note.startTime);
    float bottomY = timeToY(note.endTime);

    // Skip if completely out of view
    if (bottomY < 0 || topY > catchLineY)
      continue;

    // Clamp to visible area
    topY = juce::jmax(0.0f, topY);
    bottomY = juce::jmin(catchLineY, bottomY);

    float noteHeight = bottomY - topY;
    if (noteHeight < 2.0f)
      noteHeight = 2.0f;

    auto noteBounds = juce::Rectangle<float>(x + 2, topY, noteWidth - 4, noteHeight);

    // Get note color
    juce::Colour noteColor = getNoteColor(note);

    // Check if note is currently playing (at catch line)
    bool isPlaying = note.isPlayingAt(playbackPosition);
    if (isPlaying) {
      // Pulse effect for active notes
      float pulse = 0.5f + 0.5f * std::sin(pulsePhase);
      noteColor = noteColor.brighter(0.4f * pulse);
    }

    // Draw note with rounded corners
    g.setColour(noteColor);
    g.fillRoundedRectangle(noteBounds, 4.0f);

    // Subtle border
    g.setColour(noteColor.darker(0.3f));
    g.drawRoundedRectangle(noteBounds, 4.0f, 1.0f);

    // Velocity gradient (brighter at top)
    if (showVelocity && noteHeight > 10) {
      float velocityBrightness = static_cast<float>(note.velocity) / 127.0f * 0.3f;
      juce::ColourGradient velGradient(noteColor.brighter(velocityBrightness), noteBounds.getX(), noteBounds.getY(),
                                       noteColor, noteBounds.getX(), noteBounds.getBottom(), false);
      g.setGradientFill(velGradient);
      g.fillRoundedRectangle(noteBounds.reduced(1), 3.0f);
    }
  }
}

void NoteWaterfall::drawActiveNotesGlow(juce::Graphics &g) {
  auto bounds = getLocalBounds();
  int numNotes = highNoteNumber - lowNoteNumber + 1;
  float noteWidth = static_cast<float>(bounds.getWidth()) / static_cast<float>(numNotes);
  float catchLineY = static_cast<float>(bounds.getHeight()) * 0.9f;

  for (const auto &note : notes) {
    // Only show glow for currently playing notes
    if (!note.isPlayingAt(playbackPosition))
      continue;

    if (note.noteNumber < lowNoteNumber || note.noteNumber > highNoteNumber)
      continue;

    float x = noteToX(note.noteNumber);
    juce::Colour noteColor = getNoteColor(note);

    // Pulse glow
    float pulse = 0.5f + 0.5f * std::sin(pulsePhase);
    float glowRadius = 20.0f + pulse * 10.0f;

    // Draw radial glow at catch line
    juce::Point<float> glowCenter(x + noteWidth / 2, catchLineY);

    juce::ColourGradient glowGradient(noteColor.withAlpha(0.6f * pulse), glowCenter, noteColor.withAlpha(0.0f),
                                      glowCenter.translated(0, -glowRadius), true);
    g.setGradientFill(glowGradient);

    auto glowBounds = juce::Rectangle<float>(x - 5, catchLineY - glowRadius, noteWidth + 10, glowRadius);
    g.fillRect(glowBounds);

    // Bright line at catch point
    g.setColour(noteColor.withAlpha(0.8f));
    g.fillRect(x + 2, catchLineY - 3, noteWidth - 4, 6.0f);
  }
}

//==============================================================================
bool NoteWaterfall::isBlackKey(int noteNumber) const {
  int noteInOctave = noteNumber % 12;
  return noteInOctave == 1 || noteInOctave == 3 || noteInOctave == 6 || noteInOctave == 8 || noteInOctave == 10;
}

juce::String NoteWaterfall::getNoteName(int noteNumber) const {
  static const char *noteNames[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
  int octave = (noteNumber / 12) - 1;
  int noteIndex = noteNumber % 12;
  return juce::String(noteNames[noteIndex]) + juce::String(octave);
}

float NoteWaterfall::noteToX(int noteNumber) const {
  int numNotes = highNoteNumber - lowNoteNumber + 1;
  float noteWidth = static_cast<float>(getWidth()) / static_cast<float>(numNotes);

  int noteIndex = noteNumber - lowNoteNumber;
  return static_cast<float>(noteIndex) * noteWidth;
}

float NoteWaterfall::timeToY(double time) const {
  // Notes fall from top (future) to catch line (current time)
  // At playbackPosition, Y = catchLineY (90% of height)
  // At playbackPosition + lookaheadTime, Y = 0 (top)

  float catchLineY = static_cast<float>(getHeight()) * 0.9f;

  if (lookaheadTime <= 0)
    return catchLineY;

  double relativeTime = time - playbackPosition;
  float progress = static_cast<float>(relativeTime / lookaheadTime);

  // progress: 0 = at playback position (catch line), 1 = at top (lookahead)
  // Y: catchLineY at progress 0, 0 at progress 1
  float y = catchLineY * (1.0f - progress);

  return y;
}

juce::Colour NoteWaterfall::getNoteColor(const Note &note) const {
  if (showChannels) {
    return getChannelColor(note.channel);
  }

  if (showVelocity) {
    // Interpolate color based on velocity (brighter = louder)
    float velocityNorm = static_cast<float>(note.velocity) / 127.0f;
    return WaterfallColors::noteDefault.interpolatedWith(WaterfallColors::noteActive, velocityNorm);
  }

  return WaterfallColors::noteDefault;
}

juce::Colour NoteWaterfall::getChannelColor(int channel) const {
  int numColors = sizeof(WaterfallColors::channelColors) / sizeof(WaterfallColors::channelColors[0]);
  return WaterfallColors::channelColors[channel % numColors];
}

void NoteWaterfall::parseMIDIEvents(const juce::var &events) {
  if (!events.isArray())
    return;

  // Track active notes: (channel << 8 | note) -> Note with start time set
  std::map<int, Note> activeNotes;

  auto *eventsArray = events.getArray();
  for (const auto &eventVar : *eventsArray) {
    double time = static_cast<double>(eventVar["time"]);
    juce::String type = eventVar["type"].toString();
    int noteNum = static_cast<int>(eventVar["note"]);
    int velocity = static_cast<int>(eventVar["velocity"]);
    int channel = static_cast<int>(eventVar["channel"]);

    int noteKey = (channel << 8) | noteNum;

    if (type == "note_on" && velocity > 0) {
      Note note;
      note.startTime = time;
      note.endTime = time; // Will be updated on note_off
      note.noteNumber = noteNum;
      note.velocity = velocity;
      note.channel = channel;
      activeNotes[noteKey] = note;
    } else if (type == "note_off" || (type == "note_on" && velocity == 0)) {
      auto it = activeNotes.find(noteKey);
      if (it != activeNotes.end()) {
        it->second.endTime = time;
        notes.push_back(it->second);
        activeNotes.erase(it);
      }
    }
  }

  // Handle any notes still active at end
  for (auto &pair : activeNotes) {
    pair.second.endTime = totalDuration;
    notes.push_back(pair.second);
  }

  // Sort by start time
  std::sort(notes.begin(), notes.end(), [](const Note &a, const Note &b) { return a.startTime < b.startTime; });
}
