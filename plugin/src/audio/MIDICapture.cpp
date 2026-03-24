#include "MIDICapture.h"
#include "../util/Log.h"
#include <cmath>
#include <limits>
#include <nlohmann/json.hpp>
#include <set>

// ==============================================================================
// MIDIEvent JSON serialization
// ==============================================================================
void to_json(nlohmann::json &j, const MIDIEvent &event) {
  j = nlohmann::json{{"time", event.time},
                     {"type", event.type.toStdString()},
                     {"note", event.note},
                     {"velocity", event.velocity},
                     {"channel", event.channel}};
}

void from_json(const nlohmann::json &j, MIDIEvent &event) {
  event.time = j.value("time", 0.0);
  if (j.contains("type") && j["type"].is_string()) {
    event.type = juce::String(j["type"].get<std::string>());
  } else {
    event.type = "note_on";
  }
  event.note = j.value("note", 0);
  event.velocity = j.value("velocity", 0);
  event.channel = j.value("channel", 0);
}

// ==============================================================================
// MIDIData JSON serialization
// ==============================================================================
void to_json(nlohmann::json &j, const MIDIData &data) {
  j = data.toJson();
}

void from_json(const nlohmann::json &j, MIDIData &data) {
  data = MIDIData::fromJson(j);
}

nlohmann::json MIDIData::toJson() const {
  nlohmann::json j;
  j["total_time"] = totalTime;
  j["tempo"] = tempo;
  j["time_signature"] = nlohmann::json::array({timeSignatureNumerator, timeSignatureDenominator});

  nlohmann::json eventsArray = nlohmann::json::array();
  for (const auto &event : events) {
    nlohmann::json eventObj;
    to_json(eventObj, event);
    eventsArray.push_back(eventObj);
  }
  j["events"] = eventsArray;

  return j;
}

MIDIData MIDIData::fromJson(const nlohmann::json &j) {
  MIDIData data;

  data.totalTime = j.value("total_time", 0.0);
  data.tempo = j.value("tempo", 120.0);

  if (j.contains("time_signature") && j["time_signature"].is_array() && j["time_signature"].size() >= 2) {
    data.timeSignatureNumerator = j["time_signature"][0].get<int>();
    data.timeSignatureDenominator = j["time_signature"][1].get<int>();
  }

  if (j.contains("events") && j["events"].is_array()) {
    for (const auto &eventObj : j["events"]) {
      MIDIEvent event;
      from_json(eventObj, event);
      data.events.push_back(event);
    }
  }

  return data;
}

// ==============================================================================
MIDICapture::MIDICapture() {}

MIDICapture::~MIDICapture() {}

// ==============================================================================
void MIDICapture::prepare(double sampleRate, int samplesPerBlock) {
  currentSampleRate = sampleRate;
  currentBlockSize = samplesPerBlock;

  reset();

  Log::info("MIDICapture prepared: " + juce::String(sampleRate) + "Hz, " + juce::String(samplesPerBlock) +
            " samples/block");
}

void MIDICapture::reset() {
  capturing.store(false);
  currentSamplePosition.store(0);
  totalTimeSeconds.store(0.0);

  juce::ScopedLock lock(eventsLock);
  midiEvents.clear();
}

// ==============================================================================
void MIDICapture::startCapture() {
  if (capturing.load()) {
    Log::warn("MIDI capture already in progress, ignoring start request");
    return;
  }

  // Reset state
  currentSamplePosition.store(0);
  totalTimeSeconds.store(0.0);

  juce::ScopedLock lock(eventsLock);
  midiEvents.clear();

  // Start capturing
  capturing.store(true);

  Log::info("Started MIDI capture");
}

std::vector<MIDIEvent> MIDICapture::stopCapture() {
  if (!capturing.load()) {
    Log::warn("MIDI capture not in progress, returning empty events");
    return std::vector<MIDIEvent>();
  }

  capturing.store(false);

  // Calculate total time
  double totalTime = samplePositionToTime(currentSamplePosition.load());
  totalTimeSeconds.store(totalTime);

  // Return copy of events
  juce::ScopedLock lock(eventsLock);
  std::vector<MIDIEvent> result = midiEvents;

  Log::info("Stopped MIDI capture: " + juce::String(result.size()) + " events, " + juce::String(totalTime, 2) +
            " seconds");

  return result;
}

// ==============================================================================
void MIDICapture::captureMIDI(const juce::MidiBuffer &midiMessages, int numSamples, double /* sampleRate */) {
  // Fast exit if not capturing (atomic read)
  if (!capturing.load(std::memory_order_relaxed))
    return;

  int currentPos = currentSamplePosition.load(std::memory_order_relaxed);

  // Process MIDI messages in the buffer
  for (const auto metadata : midiMessages) {
    auto message = metadata.getMessage();
    int sampleOffset = metadata.samplePosition;

    // Calculate absolute time from start of recording
    int absoluteSample = currentPos + sampleOffset;
    double eventTime = samplePositionToTime(absoluteSample);

    // Only capture note events (note on/off)
    if (message.isNoteOn()) {
      MIDIEvent event;
      event.time = eventTime;
      event.type = "note_on";
      event.note = message.getNoteNumber();
      event.velocity = message.getVelocity();
      event.channel = message.getChannel() - 1; // JUCE channels are 1-16, we use 0-15

      addEvent(event);
    } else if (message.isNoteOff()) {
      MIDIEvent event;
      event.time = eventTime;
      event.type = "note_off";
      event.note = message.getNoteNumber();
      event.velocity = message.getVelocity();
      event.channel = message.getChannel() - 1; // JUCE channels are 1-16, we use 0-15

      addEvent(event);
    }
    // Note: We could also capture MIDI clock messages for tempo sync,
    // but for now we'll rely on DAW BPM from AudioPlayHead
  }

  // Update sample position
  currentSamplePosition.store(currentPos + numSamples, std::memory_order_relaxed);
}

// ==============================================================================
nlohmann::json MIDICapture::getMIDIDataAsJSON() const {
  juce::ScopedLock lock(eventsLock);

  nlohmann::json midiData;
  midiData["total_time"] = totalTimeSeconds.load();

  // Convert events to JSON array
  nlohmann::json eventsArray = nlohmann::json::array();

  for (const auto &event : midiEvents) {
    nlohmann::json eventObj;
    eventObj["time"] = event.time;
    eventObj["type"] = event.type.toStdString();
    eventObj["note"] = event.note;
    eventObj["velocity"] = event.velocity;
    eventObj["channel"] = event.channel;

    eventsArray.push_back(eventObj);
  }

  midiData["events"] = eventsArray;

  // Add tempo and time signature from DAW
  midiData["time_signature"] = nlohmann::json::array({timeSignatureNumerator.load(), timeSignatureDenominator.load()});
  midiData["tempo"] = currentTempo.load();

  return midiData;
}

// ==============================================================================
// MIDI Data Processing
// ==============================================================================

std::vector<MIDIEvent> MIDICapture::normalizeTiming(const std::vector<MIDIEvent> &events) {
  if (events.empty())
    return events;

  std::vector<MIDIEvent> normalized = events;

  // Find the earliest event time
  double minTime = std::numeric_limits<double>::max();
  for (const auto &event : normalized) {
    if (event.time < minTime)
      minTime = event.time;
  }

  // Normalize all times relative to start (0.0 = start of recording)
  // and round to millisecond precision
  for (auto &event : normalized) {
    event.time -= minTime;
    // Round to millisecond precision (3 decimal places)
    event.time = std::round(event.time * 1000.0) / 1000.0;
  }

  Log::debug("MIDICapture::normalizeTiming: Normalized " + juce::String(normalized.size()) + " events, offset by " +
             juce::String(minTime, 3) + "s");

  return normalized;
}

std::vector<MIDIEvent> MIDICapture::validateEvents(const std::vector<MIDIEvent> &events) {
  std::vector<MIDIEvent> validated;
  validated.reserve(events.size());

  // Track active notes for matching note_on/note_off
  // Key: (channel << 8) | note
  std::set<int> activeNotes;

  // Track seen events for duplicate removal
  // Key: hash of (time, type, note, channel)
  std::set<std::string> seenEvents;

  for (const auto &event : events) {
    // Filter out invalid notes (outside 0-127)
    if (event.note < 0 || event.note > 127) {
      Log::debug("MIDICapture::validateEvents: Filtering invalid note " + juce::String(event.note));
      continue;
    }

    // Filter out invalid velocity
    if (event.velocity < 0 || event.velocity > 127) {
      Log::debug("MIDICapture::validateEvents: Filtering invalid velocity " + juce::String(event.velocity));
      continue;
    }

    // Filter out invalid channel (0-15)
    if (event.channel < 0 || event.channel > 15) {
      Log::debug("MIDICapture::validateEvents: Filtering invalid channel " + juce::String(event.channel));
      continue;
    }

    // Create unique key for duplicate detection
    // Include time rounded to 0.1ms to catch near-duplicates
    std::string eventKey = std::to_string(static_cast<int>(event.time * 10000)) + "_" + event.type.toStdString() + "_" +
                           std::to_string(event.note) + "_" + std::to_string(event.channel);

    // Skip duplicates
    if (seenEvents.count(eventKey) > 0) {
      Log::debug("MIDICapture::validateEvents: Skipping duplicate event");
      continue;
    }
    seenEvents.insert(eventKey);

    // Track note_on/note_off pairs
    int noteKey = (event.channel << 8) | event.note;

    if (event.type == "note_on" && event.velocity > 0) {
      activeNotes.insert(noteKey);
      validated.push_back(event);
    } else if (event.type == "note_off" || (event.type == "note_on" && event.velocity == 0)) {
      // Only add note_off if we have a matching note_on
      if (activeNotes.count(noteKey) > 0) {
        activeNotes.erase(noteKey);
        MIDIEvent noteOff = event;
        noteOff.type = "note_off"; // Normalize velocity 0 note_on to note_off
        validated.push_back(noteOff);
      } else {
        Log::debug("MIDICapture::validateEvents: Orphan note_off for note " + juce::String(event.note));
      }
    }
  }

  // Add synthetic note_off events for any notes still active at end of
  // recording (This handles the case where recording stopped mid-note)
  if (!activeNotes.empty() && !validated.empty()) {
    double endTime = validated.back().time;
    for (int noteKey : activeNotes) {
      MIDIEvent syntheticOff;
      syntheticOff.time = endTime;
      syntheticOff.type = "note_off";
      syntheticOff.note = noteKey & 0xFF;
      syntheticOff.channel = (noteKey >> 8) & 0xF;
      syntheticOff.velocity = 0;
      validated.push_back(syntheticOff);

      Log::debug("MIDICapture::validateEvents: Added synthetic note_off for note " + juce::String(syntheticOff.note) +
                 " on channel " + juce::String(syntheticOff.channel));
    }
  }

  Log::info("MIDICapture::validateEvents: Validated " + juce::String(validated.size()) + " events from " +
            juce::String(events.size()) + " input events");

  return validated;
}

nlohmann::json MIDICapture::getNormalizedMIDIDataAsJSON() const {
  juce::ScopedLock lock(eventsLock);

  // Get normalized and validated events
  auto normalized = normalizeTiming(midiEvents);
  auto validated = validateEvents(normalized);

  // Build JSON
  nlohmann::json midiData;
  midiData["total_time"] = totalTimeSeconds.load();

  // Convert events to JSON array
  nlohmann::json eventsArray = nlohmann::json::array();

  for (const auto &event : validated) {
    nlohmann::json eventObj;
    eventObj["time"] = event.time;
    eventObj["type"] = event.type.toStdString();
    eventObj["note"] = event.note;
    eventObj["velocity"] = event.velocity;
    eventObj["channel"] = event.channel;

    eventsArray.push_back(eventObj);
  }

  midiData["events"] = eventsArray;

  // Add tempo and time signature from DAW
  midiData["time_signature"] = nlohmann::json::array({timeSignatureNumerator.load(), timeSignatureDenominator.load()});
  midiData["tempo"] = currentTempo.load();

  return midiData;
}

void MIDICapture::setTimeSignature(int numerator, int denominator) {
  timeSignatureNumerator.store(numerator);
  timeSignatureDenominator.store(denominator);
}

std::pair<int, int> MIDICapture::getTimeSignature() const {
  return {timeSignatureNumerator.load(), timeSignatureDenominator.load()};
}

// ==============================================================================
void MIDICapture::addEvent(const MIDIEvent &event) {
  juce::ScopedLock lock(eventsLock);
  midiEvents.push_back(event);
}

double MIDICapture::samplePositionToTime(int samplePosition) const {
  if (currentSampleRate > 0.0)
    return static_cast<double>(samplePosition) / currentSampleRate;
  return 0.0;
}

// ==============================================================================
// Typed MIDIData accessors
// ==============================================================================

MIDIData MIDICapture::getMIDIData() const {
  juce::ScopedLock lock(eventsLock);

  MIDIData data;
  data.totalTime = totalTimeSeconds.load();
  data.tempo = currentTempo.load();
  data.timeSignatureNumerator = timeSignatureNumerator.load();
  data.timeSignatureDenominator = timeSignatureDenominator.load();
  data.events = midiEvents;

  return data;
}

MIDIData MIDICapture::getNormalizedMIDIData() const {
  juce::ScopedLock lock(eventsLock);

  // Get normalized and validated events
  auto normalized = normalizeTiming(midiEvents);
  auto validated = validateEvents(normalized);

  MIDIData data;
  data.totalTime = totalTimeSeconds.load();
  data.tempo = currentTempo.load();
  data.timeSignatureNumerator = timeSignatureNumerator.load();
  data.timeSignatureDenominator = timeSignatureDenominator.load();
  data.events = validated;

  return data;
}
