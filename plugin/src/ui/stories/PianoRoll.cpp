#include "PianoRoll.h"
#include "../../util/Colors.h"
#include "../../util/Log.h"

namespace PianoRollColors {
const juce::Colour background(0xff1a1a2e);
const juce::Colour gridLine(0xff2d2d44);
const juce::Colour gridLineBeat(0xff3d3d54);
const juce::Colour whiteKey(0xffe8e8e8);
const juce::Colour blackKey(0xff2a2a3a);
const juce::Colour keyBorder(0xff404050);
const juce::Colour noteDefault(0xff7c4dff);
const juce::Colour noteActive(0xffb388ff);
const juce::Colour playhead(0xffff5252);
const juce::Colour textPrimary(0xff888888);
} // namespace PianoRollColors

// ==============================================================================
PianoRoll::PianoRoll() {
  // Start animation timer
  startTimerHz(30);

  Log::debug("PianoRoll created");
}

PianoRoll::~PianoRoll() {
  stopTimer();
}

// ==============================================================================
void PianoRoll::paint(juce::Graphics &g) {
  // Background
  g.fillAll(PianoRollColors::background);

  // Draw grid first
  drawGridLines(g);

  // Draw notes
  drawNotes(g);

  // Draw piano keys on left
  drawPianoKeys(g);

  // Draw playhead on top
  drawPlayhead(g);
}

void PianoRoll::resized() {
  auto bounds = getLocalBounds();

  // Piano keys on left
  pianoKeyArea = bounds.removeFromLeft(pianoKeyWidth);

  // Note grid fills rest
  noteGridArea = bounds;
}

// ==============================================================================
void PianoRoll::timerCallback() {
  // Animate pulse for active notes
  pulsePhase += 0.1f;
  if (pulsePhase > juce::MathConstants<float>::twoPi)
    pulsePhase -= juce::MathConstants<float>::twoPi;

  repaint();
}

// ==============================================================================
void PianoRoll::setMIDIData(const juce::var &midiData) {
  notes.clear();

  if (!midiData.isObject()) {
    Log::warn("PianoRoll: Invalid MIDI data format");
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
    lowNoteNumber = juce::jmax(0, minNote - 5);
    highNoteNumber = juce::jmin(127, maxNote + 5);

    // Ensure at least an octave range
    if (highNoteNumber - lowNoteNumber < 12) {
      lowNoteNumber = juce::jmax(0, (minNote + maxNote) / 2 - 6);
      highNoteNumber = lowNoteNumber + 12;
    }
  }

  Log::info("PianoRoll: Loaded " + juce::String(notes.size()) + " notes, " + juce::String(totalDuration, 2) +
            "s duration");

  repaint();
}

void PianoRoll::clearMIDIData() {
  notes.clear();
  totalDuration = 0.0;
  playbackPosition = 0.0;
  repaint();
}

void PianoRoll::setPlaybackPosition(double positionSeconds) {
  playbackPosition = positionSeconds;
  repaint();
}

void PianoRoll::setNoteRange(int lowNote, int highNote) {
  lowNoteNumber = juce::jlimit(0, 126, lowNote);
  highNoteNumber = juce::jlimit(lowNoteNumber + 1, 127, highNote);
  repaint();
}

// ==============================================================================
void PianoRoll::drawPianoKeys(juce::Graphics &g) {
  int numNotes = highNoteNumber - lowNoteNumber + 1;
  float keyHeight = static_cast<float>(pianoKeyArea.getHeight()) / static_cast<float>(numNotes);

  for (int note = highNoteNumber; note >= lowNoteNumber; --note) {
    int index = highNoteNumber - note;
    float y = static_cast<float>(pianoKeyArea.getY()) + static_cast<float>(index) * keyHeight;

    auto keyBounds = juce::Rectangle<float>(static_cast<float>(pianoKeyArea.getX()), y,
                                            static_cast<float>(pianoKeyArea.getWidth()), keyHeight);

    if (isBlackKey(note)) {
      g.setColour(PianoRollColors::blackKey);
      g.fillRect(keyBounds);
    } else {
      g.setColour(PianoRollColors::whiteKey);
      g.fillRect(keyBounds);
    }

    // Key border
    g.setColour(PianoRollColors::keyBorder);
    g.drawHorizontalLine(static_cast<int>(y + keyHeight), static_cast<float>(pianoKeyArea.getX()),
                         static_cast<float>(pianoKeyArea.getRight()));

    // Note name for C notes
    if (note % 12 == 0) {
      g.setColour(PianoRollColors::textPrimary);
      g.setFont(9.0f);
      g.drawText(getNoteName(note), keyBounds.reduced(2.0f), juce::Justification::centredLeft);
    }
  }
}

void PianoRoll::drawGridLines(juce::Graphics &g) {
  if (totalDuration <= 0)
    return;

  // Vertical grid lines (beats based on tempo)
  double beatsPerSecond = tempo / 60.0;
  double secondsPerBeat = 1.0 / beatsPerSecond;

  g.setColour(PianoRollColors::gridLine);

  for (double time = 0; time < totalDuration; time += secondsPerBeat) {
    float x = timeToX(time);
    if (x >= static_cast<float>(noteGridArea.getX()) && x <= static_cast<float>(noteGridArea.getRight())) {
      // Stronger line for bar boundaries (every 4 beats)
      int beatNum = static_cast<int>(time * beatsPerSecond);
      if (beatNum % 4 == 0) {
        g.setColour(PianoRollColors::gridLineBeat);
      } else {
        g.setColour(PianoRollColors::gridLine);
      }

      g.drawVerticalLine(static_cast<int>(x), static_cast<float>(noteGridArea.getY()),
                         static_cast<float>(noteGridArea.getBottom()));
    }
  }

  // Horizontal grid lines for each note
  int numNotes = highNoteNumber - lowNoteNumber + 1;
  float keyHeight = static_cast<float>(noteGridArea.getHeight()) / static_cast<float>(numNotes);

  g.setColour(PianoRollColors::gridLine);
  for (int i = 0; i <= numNotes; ++i) {
    float y = static_cast<float>(noteGridArea.getY()) + static_cast<float>(i) * keyHeight;
    g.drawHorizontalLine(static_cast<int>(y), static_cast<float>(noteGridArea.getX()),
                         static_cast<float>(noteGridArea.getRight()));
  }
}

void PianoRoll::drawNotes(juce::Graphics &g) {
  int numNotes = highNoteNumber - lowNoteNumber + 1;
  float keyHeight = static_cast<float>(noteGridArea.getHeight()) / static_cast<float>(numNotes);

  for (size_t i = 0; i < notes.size(); ++i) {
    const auto &note = notes[i];
    bool isHovered = (static_cast<int>(i) == hoveredNoteIndex);

    // Check if note is in visible range
    if (note.noteNumber < lowNoteNumber || note.noteNumber > highNoteNumber)
      continue;

    float x1 = timeToX(note.startTime);
    float x2 = timeToX(note.endTime);
    float y = noteToY(note.noteNumber);

    // Minimum width for visibility
    float noteWidth = juce::jmax(4.0f, x2 - x1);

    auto noteBounds = juce::Rectangle<float>(x1, y, noteWidth, keyHeight - 2);

    // Clip to grid area
    noteBounds = noteBounds.getIntersection(noteGridArea.toFloat());
    if (noteBounds.isEmpty())
      continue;

    // Get note color
    juce::Colour noteColor = getNoteColor(note);

    // Check if note is currently playing
    bool isPlaying = note.isPlayingAt(playbackPosition);
    if (isPlaying) {
      // Pulse effect for active notes
      float pulse = 0.5f + 0.5f * std::sin(pulsePhase);
      noteColor = noteColor.brighter(0.3f * pulse);
    }

    // Highlight hovered note
    if (isHovered) {
      g.setColour(noteColor.brighter(0.5f));
      g.fillRoundedRectangle(noteBounds.expanded(2.0f), 3.0f);
    }

    // Draw note rectangle
    g.setColour(noteColor);
    g.fillRoundedRectangle(noteBounds, 3.0f);

    // Border (thicker for hovered)
    g.setColour(noteColor.darker(0.3f));
    g.drawRoundedRectangle(noteBounds, 3.0f, isHovered ? 2.0f : 1.0f);

    // Velocity indicator (darker left edge)
    if (showVelocity && noteBounds.getWidth() > 8.0f) {
      float velocityHeight = (static_cast<float>(note.velocity) / 127.0f) * noteBounds.getHeight();
      auto velocityBounds = noteBounds.withHeight(velocityHeight).withY(noteBounds.getBottom() - velocityHeight);
      g.setColour(noteColor.brighter(0.2f));
      g.fillRect(velocityBounds.removeFromLeft(3.0f));
    }
  }

  // Draw tooltip for hovered note
  if (hoveredNoteIndex >= 0 && hoveredNoteIndex < static_cast<int>(notes.size())) {
    const auto &note = notes[static_cast<size_t>(hoveredNoteIndex)];
    float x = timeToX(note.startTime);
    float y = noteToY(note.noteNumber);

    juce::String tooltip = getNoteName(note.noteNumber) + " (" + juce::String(note.velocity) + " vel)";
    auto tooltipBounds = juce::Rectangle<int>(static_cast<int>(x), static_cast<int>(y - 30.0f), 100, 25);

    // Background
    g.setColour(juce::Colour(0xff000000).withAlpha(0.8f));
    g.fillRoundedRectangle(tooltipBounds.toFloat(), 4.0f);

    // Text
    g.setColour(juce::Colours::white);
    g.setFont(12.0f);
    g.drawText(tooltip, tooltipBounds, juce::Justification::centred);
  }
}

void PianoRoll::drawPlayhead(juce::Graphics &g) {
  if (totalDuration <= 0)
    return;

  float x = timeToX(playbackPosition);

  if (x >= static_cast<float>(noteGridArea.getX()) && x <= static_cast<float>(noteGridArea.getRight())) {
    g.setColour(PianoRollColors::playhead);
    g.drawVerticalLine(static_cast<int>(x), static_cast<float>(getLocalBounds().getY()),
                       static_cast<float>(getLocalBounds().getBottom()));

    // Playhead top marker
    juce::Path triangle;
    triangle.addTriangle(x - 5.0f, 0.0f, x + 5.0f, 0.0f, x, 8.0f);
    g.fillPath(triangle);
  }
}

// ==============================================================================
bool PianoRoll::isBlackKey(int noteNumber) const {
  int noteInOctave = noteNumber % 12;
  return noteInOctave == 1 || noteInOctave == 3 || noteInOctave == 6 || noteInOctave == 8 || noteInOctave == 10;
}

juce::String PianoRoll::getNoteName(int noteNumber) const {
  static const char *noteNames[] = {"C", "C# ", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
  int octave = (noteNumber / 12) - 1;
  int noteIndex = noteNumber % 12;
  return juce::String(noteNames[noteIndex]) + juce::String(octave);
}

float PianoRoll::noteToY(int noteNumber) const {
  int numNotes = highNoteNumber - lowNoteNumber + 1;
  float keyHeight = static_cast<float>(noteGridArea.getHeight()) / static_cast<float>(numNotes);

  // High notes at top, low notes at bottom
  int noteIndex = highNoteNumber - noteNumber;
  return static_cast<float>(noteGridArea.getY()) + static_cast<float>(noteIndex) * keyHeight + 1.0f;
}

float PianoRoll::timeToX(double time) const {
  if (totalDuration <= 0)
    return static_cast<float>(noteGridArea.getX());

  // Account for zoom and scroll
  double visibleDuration = totalDuration / zoomLevel;
  double adjustedTime = (time - timelineScrollOffset) * zoomLevel;

  float width = static_cast<float>(noteGridArea.getWidth());
  float progress = static_cast<float>(adjustedTime / visibleDuration);
  progress = juce::jlimit(0.0f, 1.0f, progress);
  return static_cast<float>(noteGridArea.getX()) + progress * width;
}

double PianoRoll::xToTime(float x) const {
  if (totalDuration <= 0 || noteGridArea.getWidth() <= 0)
    return 0.0;

  float relativeX = x - static_cast<float>(noteGridArea.getX());
  float progress = relativeX / static_cast<float>(noteGridArea.getWidth());
  progress = juce::jlimit(0.0f, 1.0f, progress);

  double visibleDuration = totalDuration / zoomLevel;
  return timelineScrollOffset + (static_cast<double>(progress) * visibleDuration);
}

int PianoRoll::findNoteAt(juce::Point<int> position) const {
  if (!noteGridArea.contains(position))
    return -1;

  int numNotes = highNoteNumber - lowNoteNumber + 1;
  float keyHeight = static_cast<float>(noteGridArea.getHeight()) / static_cast<float>(numNotes);
  float relativeY = static_cast<float>(position.y) - static_cast<float>(noteGridArea.getY());
  int noteIndex = static_cast<int>(relativeY / keyHeight);
  int noteNumber = highNoteNumber - noteIndex;

  // Find notes near this position
  for (size_t i = 0; i < notes.size(); ++i) {
    const auto &note = notes[i];
    if (note.noteNumber == noteNumber) {
      // Check if click is within note's time range
      float x1 = timeToX(note.startTime);
      float x2 = timeToX(note.endTime);
      if (static_cast<float>(position.x) >= x1 && static_cast<float>(position.x) <= x2) {
        return static_cast<int>(i);
      }
    }
  }

  return -1;
}

juce::Colour PianoRoll::getNoteColor(const Note &note) const {
  if (showChannels) {
    return getChannelColor(note.channel);
  }

  if (showVelocity) {
    // Interpolate color based on velocity
    float velocityNorm = static_cast<float>(note.velocity) / 127.0f;
    return PianoRollColors::noteDefault.interpolatedWith(PianoRollColors::noteActive, velocityNorm);
  }

  return PianoRollColors::noteDefault;
}

juce::Colour PianoRoll::getChannelColor(int channel) const {
  return SidechainColors::getMidiNoteColor(channel);
}

void PianoRoll::parseMIDIEvents(const juce::var &events) {
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

// ==============================================================================
void PianoRoll::mouseMove(const juce::MouseEvent &event) {
  int noteIndex = findNoteAt(event.getPosition());
  if (noteIndex != hoveredNoteIndex) {
    hoveredNoteIndex = noteIndex;
    repaint();
  }
  lastMousePosition = event.getPosition();
}

void PianoRoll::mouseExit(const juce::MouseEvent & /*event*/) {
  if (hoveredNoteIndex >= 0) {
    hoveredNoteIndex = -1;
    repaint();
  }
}

void PianoRoll::mouseUp(const juce::MouseEvent &event) {
  if (!noteGridArea.contains(event.getPosition()))
    return;

  // Check if clicking on a note
  int noteIndex = findNoteAt(event.getPosition());
  if (noteIndex >= 0 && noteIndex < static_cast<int>(notes.size())) {
    // Seek to note start time
    if (onSeekToTime)
      onSeekToTime(notes[static_cast<size_t>(noteIndex)].startTime);
    return;
  }

  // Otherwise, seek to clicked time position
  double clickTime = xToTime(static_cast<float>(event.getPosition().x));
  if (onSeekToTime)
    onSeekToTime(clickTime);
}

void PianoRoll::mouseWheelMove(const juce::MouseEvent &event, const juce::MouseWheelDetails &wheel) {
  if (!noteGridArea.contains(event.getPosition()))
    return;

  // Zoom with Ctrl/Cmd + wheel
  if (event.mods.isCommandDown() || event.mods.isCtrlDown()) {
    double zoomFactor = wheel.deltaY > 0 ? 1.1 : 0.9;
    zoomLevel = juce::jlimit(0.5, 5.0, zoomLevel * zoomFactor);
    repaint();
  } else {
    // Scroll timeline with wheel
    double scrollSpeed = totalDuration * 0.1; // 10% of duration per scroll
    timelineScrollOffset += wheel.deltaY * scrollSpeed;
    timelineScrollOffset = juce::jlimit(0.0, totalDuration * (1.0 - 1.0 / zoomLevel), timelineScrollOffset);
    repaint();
  }
}
