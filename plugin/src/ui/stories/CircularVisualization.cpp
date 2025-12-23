#include "CircularVisualization.h"
#include "../../util/Colors.h"
#include "../../util/Json.h"
#include "../../util/Log.h"

namespace CircularColors {
// Colors that don't have direct equivalents in SidechainColors
const juce::Colour background(0xff0a0a14);
const juce::Colour ringLine(0xff1a1a2e);
const juce::Colour sweepLine(0xffff5252);
const juce::Colour sweepGlow(0x40ff5252);
const juce::Colour noteActive(0xffb388ff);

// Use SidechainColors for common colors
inline juce::Colour noteDefault() { return SidechainColors::getMidiNoteColor(0); }
inline juce::Colour centerText() { return SidechainColors::textMuted(); }
} // namespace CircularColors

// ==============================================================================
CircularVisualization::CircularVisualization() {
  // Start animation timer
  startTimerHz(60);

  Log::debug("CircularVisualization created");
}

CircularVisualization::~CircularVisualization() {
  stopTimer();
}

// ==============================================================================
void CircularVisualization::paint(juce::Graphics &g) {
  // Background
  drawBackground(g);

  // Draw concentric rings
  drawRings(g);

  // Draw notes (arcs or dots)
  drawNotes(g);

  // Draw sweep line
  drawSweepLine(g);

  // Draw active notes with glow
  drawActiveNotes(g);

  // Draw center info
  drawCenterInfo(g);
}

void CircularVisualization::resized() {
  auto bounds = getLocalBounds().toFloat();

  // Calculate center and radii
  center = bounds.getCentre();

  float minDimension = juce::jmin(bounds.getWidth(), bounds.getHeight());
  outerRadius = minDimension * 0.45f;
  innerRadius = outerRadius * 0.2f;
}

// ==============================================================================
void CircularVisualization::timerCallback() {
  // Animate pulse for active notes
  pulsePhase += 0.15f;
  if (pulsePhase > juce::MathConstants<float>::twoPi)
    pulsePhase -= juce::MathConstants<float>::twoPi;

  repaint();
}

// ==============================================================================
void CircularVisualization::setMIDIData(const juce::var &midiData) {
  notes.clear();

  if (!midiData.isObject()) {
    Log::warn("CircularVisualization: Invalid MIDI data format");
    return;
  }

  // Get total duration
  totalDuration = Json::getDouble(midiData, "total_time", 0.0);
  tempo = Json::getDouble(midiData, "tempo", 120.0);

  // Parse events
  auto events = Json::getArray(midiData, "events");
  if (Json::isArray(events)) {
    parseMIDIEvents(events);
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

  Log::info("CircularVisualization: Loaded " + juce::String(notes.size()) + " notes, " +
            juce::String(totalDuration, 2) + "s duration");

  repaint();
}

void CircularVisualization::clearMIDIData() {
  notes.clear();
  totalDuration = 0.0;
  playbackPosition = 0.0;
  repaint();
}

void CircularVisualization::setPlaybackPosition(double positionSeconds) {
  playbackPosition = positionSeconds;
  repaint();
}

// ==============================================================================
void CircularVisualization::drawBackground(juce::Graphics &g) {
  // Radial gradient background
  juce::ColourGradient gradient(CircularColors::background.brighter(0.1f), center,
                                CircularColors::background.darker(0.3f), center.translated(outerRadius, outerRadius),
                                true);
  g.setGradientFill(gradient);
  g.fillAll();
}

void CircularVisualization::drawRings(juce::Graphics &g) {
  // Draw concentric rings for pitch reference
  constexpr float numRings = 5.0f;
  float radiusStep = (outerRadius - innerRadius) / numRings;

  g.setColour(CircularColors::ringLine);

  for (int i = 0; i <= static_cast<int>(numRings); ++i) {
    float radius = innerRadius + static_cast<float>(i) * radiusStep;
    g.drawEllipse(center.x - radius, center.y - radius, radius * 2, radius * 2, 1.0f);
  }

  // Draw radial lines every 30 degrees
  for (int i = 0; i < 12; ++i) {
    float angle =
        static_cast<float>(i) * juce::MathConstants<float>::twoPi / 12.0f - juce::MathConstants<float>::halfPi;
    auto innerPoint = polarToCartesian(angle, innerRadius);
    auto outerPoint = polarToCartesian(angle, outerRadius);
    g.drawLine(innerPoint.x, innerPoint.y, outerPoint.x, outerPoint.y, 1.0f);
  }
}

void CircularVisualization::drawNotes(juce::Graphics &g) {
  for (const auto &note : notes) {
    // Skip currently playing notes (drawn separately with glow)
    if (note.isPlayingAt(playbackPosition))
      continue;

    // Check if note is in visible range
    if (note.noteNumber < lowNoteNumber || note.noteNumber > highNoteNumber)
      continue;

    float startAngle = timeToAngle(note.startTime);
    float endAngle = timeToAngle(note.endTime);
    float radius = noteToRadius(note.noteNumber);

    // Get note color
    juce::Colour noteColor = getNoteColor(note);

    // Draw based on style
    if (visualStyle == Style::Dots) {
      // Draw dot at note position
      auto pos = polarToCartesian(startAngle, radius);
      float dotSize = showVelocity ? (static_cast<float>(note.velocity) / 127.0f * 8.0f + 4.0f) : 6.0f;
      g.setColour(noteColor);
      g.fillEllipse(pos.x - dotSize / 2, pos.y - dotSize / 2, dotSize, dotSize);
    } else if (visualStyle == Style::Arcs) {
      // Draw arc for note duration
      float arcThickness = showVelocity ? (static_cast<float>(note.velocity) / 127.0f * 6.0f + 3.0f) : 5.0f;

      juce::Path arcPath;
      arcPath.addCentredArc(center.x, center.y, radius, radius, 0.0f, startAngle, endAngle, true);

      g.setColour(noteColor.withAlpha(0.7f));
      g.strokePath(arcPath,
                   juce::PathStrokeType(arcThickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    } else // Particles
    {
      // Draw multiple small dots along the note duration
      int numParticles = juce::jmax(3, static_cast<int>((endAngle - startAngle) * 10));
      float particleSize = showVelocity ? (static_cast<float>(note.velocity) / 127.0f * 4.0f + 2.0f) : 3.0f;

      g.setColour(noteColor.withAlpha(0.5f));
      for (int i = 0; i < numParticles; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(numParticles - 1);
        float angle = startAngle + t * (endAngle - startAngle);
        auto pos = polarToCartesian(angle, radius);
        g.fillEllipse(pos.x - particleSize / 2, pos.y - particleSize / 2, particleSize, particleSize);
      }
    }
  }
}

void CircularVisualization::drawSweepLine(juce::Graphics &g) {
  if (totalDuration <= 0)
    return;

  float angle = timeToAngle(playbackPosition);

  // Draw glow
  for (int i = 10; i >= 0; --i) {
    float alpha = 0.05f * static_cast<float>(10 - i);
    float glowAngle = angle - static_cast<float>(i) * 0.02f;
    auto innerPoint = polarToCartesian(glowAngle, innerRadius);
    auto outerPoint = polarToCartesian(glowAngle, outerRadius + 10);

    g.setColour(CircularColors::sweepLine.withAlpha(alpha));
    g.drawLine(innerPoint.x, innerPoint.y, outerPoint.x, outerPoint.y, 2.0f);
  }

  // Draw main sweep line
  auto innerPoint = polarToCartesian(angle, innerRadius - 5);
  auto outerPoint = polarToCartesian(angle, outerRadius + 15);

  g.setColour(CircularColors::sweepLine);
  g.drawLine(innerPoint.x, innerPoint.y, outerPoint.x, outerPoint.y, 2.0f);

  // Draw dot at sweep line tip
  g.fillEllipse(outerPoint.x - 4, outerPoint.y - 4, 8, 8);
}

void CircularVisualization::drawActiveNotes(juce::Graphics &g) {
  for (const auto &note : notes) {
    if (!note.isPlayingAt(playbackPosition))
      continue;

    if (note.noteNumber < lowNoteNumber || note.noteNumber > highNoteNumber)
      continue;

    float startAngle = timeToAngle(note.startTime);
    // Note: endAngle not currently used in visualization, but could be used for note duration arcs
    float currentAngle = timeToAngle(playbackPosition);
    float radius = noteToRadius(note.noteNumber);

    juce::Colour noteColor = getNoteColor(note);

    // Draw glow around active note
    float pulse = 0.5f + 0.5f * std::sin(pulsePhase);
    float glowSize = 15.0f + pulse * 10.0f;

    auto pos = polarToCartesian(currentAngle, radius);

    // Radial glow
    juce::ColourGradient glowGradient(noteColor.withAlpha(0.8f * pulse), pos, noteColor.withAlpha(0.0f),
                                      pos.translated(glowSize, 0), true);
    g.setGradientFill(glowGradient);
    g.fillEllipse(pos.x - glowSize, pos.y - glowSize, glowSize * 2, glowSize * 2);

    // Draw the arc up to current position
    if (visualStyle == Style::Arcs || visualStyle == Style::Particles) {
      float arcThickness = showVelocity ? (static_cast<float>(note.velocity) / 127.0f * 8.0f + 4.0f) : 6.0f;

      juce::Path arcPath;
      arcPath.addCentredArc(center.x, center.y, radius, radius, 0.0f, startAngle, currentAngle, true);

      g.setColour(noteColor.brighter(0.3f));
      g.strokePath(arcPath,
                   juce::PathStrokeType(arcThickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Draw bright dot at current position
    float dotSize = showVelocity ? (static_cast<float>(note.velocity) / 127.0f * 12.0f + 8.0f) : 10.0f;
    g.setColour(noteColor.brighter(0.5f));
    g.fillEllipse(pos.x - dotSize / 2, pos.y - dotSize / 2, dotSize, dotSize);

    // Draw outline
    g.setColour(juce::Colours::white.withAlpha(0.5f));
    g.drawEllipse(pos.x - dotSize / 2, pos.y - dotSize / 2, dotSize, dotSize, 1.5f);
  }
}

void CircularVisualization::drawCenterInfo(juce::Graphics &g) {
  // Draw time in center
  float centerSize = innerRadius * 0.8f;
  auto centerBounds =
      juce::Rectangle<float>(center.x - centerSize, center.y - centerSize / 2, centerSize * 2, centerSize);

  // Format time as MM:SS
  int seconds = static_cast<int>(playbackPosition);
  int minutes = seconds / 60;
  seconds = seconds % 60;
  juce::String timeStr = juce::String::formatted("%d:%02d", minutes, seconds);

  g.setColour(CircularColors::centerText());
  g.setFont(centerSize * 0.4f);
  g.drawText(timeStr, centerBounds, juce::Justification::centred);

  // Draw total duration below
  if (totalDuration > 0) {
    int totalSec = static_cast<int>(totalDuration);
    int totalMin = totalSec / 60;
    totalSec = totalSec % 60;
    juce::String totalStr = "/ " + juce::String::formatted("%d:%02d", totalMin, totalSec);

    auto totalBounds = centerBounds.translated(0, centerSize * 0.4f);
    g.setFont(centerSize * 0.25f);
    g.setColour(CircularColors::centerText().darker(0.3f));
    g.drawText(totalStr, totalBounds, juce::Justification::centred);
  }
}

// ==============================================================================
float CircularVisualization::timeToAngle(double time) const {
  if (totalDuration <= 0)
    return -juce::MathConstants<float>::halfPi; // Start at top (12 o'clock)

  // Time maps to angle: 0 = top, progress clockwise
  float progress = static_cast<float>(time / totalDuration);
  return -juce::MathConstants<float>::halfPi + progress * juce::MathConstants<float>::twoPi;
}

float CircularVisualization::noteToRadius(int noteNumber) const {
  // Map note number to radius: low notes = inner, high notes = outer
  int noteRange = highNoteNumber - lowNoteNumber;
  if (noteRange <= 0)
    return innerRadius;

  float normalizedNote = static_cast<float>(noteNumber - lowNoteNumber) / static_cast<float>(noteRange);
  return innerRadius + normalizedNote * (outerRadius - innerRadius);
}

juce::Point<float> CircularVisualization::polarToCartesian(float angle, float radius) const {
  return juce::Point<float>(center.x + radius * std::cos(angle), center.y + radius * std::sin(angle));
}

juce::Colour CircularVisualization::getNoteColor(const Note &note) const {
  if (showChannels) {
    return getChannelColor(note.channel);
  }

  if (showVelocity) {
    // Interpolate color based on velocity
    float velocityNorm = static_cast<float>(note.velocity) / 127.0f;
    return CircularColors::noteDefault().interpolatedWith(CircularColors::noteActive, velocityNorm);
  }

  return CircularColors::noteDefault();
}

juce::Colour CircularVisualization::getChannelColor(int channel) const {
  // Use centralized MIDI note colors from SidechainColors
  return SidechainColors::getMidiNoteColor(channel);
}

void CircularVisualization::parseMIDIEvents(const juce::var &events) {
  if (!Json::isArray(events))
    return;

  // Track active notes: (channel << 8 | note) -> Note with start time set
  std::map<int, Note> activeNotes;

  auto *eventsArray = events.getArray();
  for (const auto &eventVar : *eventsArray) {
    double time = Json::getDouble(eventVar, "time", 0.0);
    juce::String type = Json::getString(eventVar, "type");
    int noteNum = Json::getInt(eventVar, "note", 0);
    int velocity = Json::getInt(eventVar, "velocity", 0);
    int channel = Json::getInt(eventVar, "channel", 0);

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
