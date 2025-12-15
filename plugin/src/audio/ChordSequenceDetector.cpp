#include "ChordSequenceDetector.h"

//==============================================================================
ChordSequenceDetector::ChordSequenceDetector() {
  currentChord.rootNote = -1;
  currentChord.type = ChordType::Unknown;
  lastDetectedChord.rootNote = -1;
  lastDetectedChord.type = ChordType::Unknown;
}

//==============================================================================
void ChordSequenceDetector::processMIDI(const juce::MidiBuffer &midiMessages, double /* sampleRate */) {
  if (!detecting.load())
    return;

  bool notesChanged = false;
  double currentTime = juce::Time::getMillisecondCounterHiRes() / 1000.0;

  // Process all MIDI messages
  for (const auto metadata : midiMessages) {
    const auto msg = metadata.getMessage();

    if (msg.isNoteOn()) {
      currentlyHeldNotes.insert(msg.getNoteNumber());
      notesChanged = true;
    } else if (msg.isNoteOff()) {
      currentlyHeldNotes.erase(msg.getNoteNumber());
      notesChanged = true;
    }
  }

  // Check for chord timeout (reset history if too much time passed)
  if (currentTime - lastChordTime > chordTimeout && !chordHistory.empty()) {
    juce::ScopedLock lock(chordLock);
    chordHistory.clear();
  }

  // Only analyze if notes changed and we have 3+ notes (a chord)
  if (notesChanged && currentlyHeldNotes.size() >= 3) {
    Chord detected = identifyChord(currentlyHeldNotes);
    detected.timestamp = currentTime;

    // Only add to history if it's a new chord (different from last)
    if (detected.type != ChordType::Unknown && !(detected == lastDetectedChord)) {
      {
        juce::ScopedLock lock(chordLock);
        currentChord = detected;

        // Add to history
        chordHistory.push_back(detected);
        if (chordHistory.size() > maxHistorySize) {
          chordHistory.erase(chordHistory.begin());
        }
      }

      lastDetectedChord = detected;
      lastChordTime = currentTime;

      // Check all unlock sequences
      juce::ScopedLock seqLock(sequenceLock);
      for (auto &sequence : unlockSequences) {
        if (!sequence.unlocked && checkSequenceMatch(sequence)) {
          triggerUnlock(sequence);
        }
      }
    }
  } else if (currentlyHeldNotes.size() < 3) {
    // Clear current chord when not enough notes
    juce::ScopedLock lock(chordLock);
    currentChord.rootNote = -1;
    currentChord.type = ChordType::Unknown;
    currentChord.notes.clear();
  }
}

//==============================================================================
ChordSequenceDetector::Chord ChordSequenceDetector::identifyChord(const std::set<int> &heldNotes) {
  Chord result;
  result.rootNote = -1;
  result.type = ChordType::Unknown;

  if (heldNotes.size() < 3)
    return result;

  // Convert to pitch classes
  std::set<int> pitchClasses;
  for (int note : heldNotes) {
    pitchClasses.insert(getPitchClass(note));
    result.notes.insert(getPitchClass(note));
  }

  // Identify chord type and root
  int root = 0;
  result.type = identifyChordType(pitchClasses, root);
  result.rootNote = root;

  return result;
}

//==============================================================================
ChordSequenceDetector::ChordType ChordSequenceDetector::identifyChordType(const std::set<int> &pitchClasses,
                                                                          int &outRoot) {
  // Try each pitch class as potential root
  for (int root = 0; root < 12; root++) {
    if (pitchClasses.find(root) == pitchClasses.end())
      continue; // Root must be in the chord

    // Calculate intervals from root
    std::set<int> intervals;
    for (int pc : pitchClasses) {
      int interval = (pc - root + 12) % 12;
      intervals.insert(interval);
    }

    // Check for chord patterns (intervals from root)
    // Major: 0, 4, 7
    if (intervals.count(0) && intervals.count(4) && intervals.count(7)) {
      outRoot = root;
      if (intervals.count(11))
        return ChordType::Major7;
      if (intervals.count(10))
        return ChordType::Dominant7;
      return ChordType::Major;
    }

    // Minor: 0, 3, 7
    if (intervals.count(0) && intervals.count(3) && intervals.count(7)) {
      outRoot = root;
      if (intervals.count(10))
        return ChordType::Minor7;
      return ChordType::Minor;
    }

    // Diminished: 0, 3, 6
    if (intervals.count(0) && intervals.count(3) && intervals.count(6)) {
      outRoot = root;
      return ChordType::Diminished;
    }

    // Augmented: 0, 4, 8
    if (intervals.count(0) && intervals.count(4) && intervals.count(8)) {
      outRoot = root;
      return ChordType::Augmented;
    }

    // Sus4: 0, 5, 7
    if (intervals.count(0) && intervals.count(5) && intervals.count(7)) {
      outRoot = root;
      return ChordType::Sus4;
    }

    // Sus2: 0, 2, 7
    if (intervals.count(0) && intervals.count(2) && intervals.count(7)) {
      outRoot = root;
      return ChordType::Sus2;
    }
  }

  return ChordType::Unknown;
}

//==============================================================================
void ChordSequenceDetector::addUnlockSequence(UnlockSequence sequence) {
  juce::ScopedLock lock(sequenceLock);
  unlockSequences.push_back(std::move(sequence));
}

void ChordSequenceDetector::clearUnlockSequences() {
  juce::ScopedLock lock(sequenceLock);
  unlockSequences.clear();
}

void ChordSequenceDetector::reset() {
  juce::ScopedLock lock(chordLock);
  currentlyHeldNotes.clear();
  chordHistory.clear();
  currentChord.rootNote = -1;
  currentChord.type = ChordType::Unknown;
  lastDetectedChord.rootNote = -1;
  lastDetectedChord.type = ChordType::Unknown;
}

//==============================================================================
ChordSequenceDetector::Chord ChordSequenceDetector::getCurrentChord() const {
  juce::ScopedLock lock(chordLock);
  return currentChord;
}

std::vector<ChordSequenceDetector::Chord> ChordSequenceDetector::getChordHistory() const {
  juce::ScopedLock lock(chordLock);
  return chordHistory;
}

bool ChordSequenceDetector::isSequenceUnlocked(const juce::String &sequenceName) const {
  juce::ScopedLock lock(sequenceLock);
  for (const auto &seq : unlockSequences) {
    if (seq.name == sequenceName)
      return seq.unlocked;
  }
  return false;
}

std::vector<juce::String> ChordSequenceDetector::getUnlockedSequences() const {
  juce::ScopedLock lock(sequenceLock);
  std::vector<juce::String> result;
  for (const auto &seq : unlockSequences) {
    if (seq.unlocked)
      result.push_back(seq.name);
  }
  return result;
}

//==============================================================================
bool ChordSequenceDetector::checkSequenceMatch(const UnlockSequence &sequence) {
  if (chordHistory.size() < sequence.chords.size())
    return false;

  // Check if the last N chords match the sequence
  size_t startIdx = chordHistory.size() - sequence.chords.size();
  for (size_t i = 0; i < sequence.chords.size(); i++) {
    if (!(chordHistory[startIdx + i] == sequence.chords[i]))
      return false;
  }

  return true;
}

void ChordSequenceDetector::triggerUnlock(UnlockSequence &sequence) {
  sequence.unlocked = true;

  // Call callback on message thread
  if (sequence.onUnlock) {
    juce::MessageManager::callAsync([callback = sequence.onUnlock]() { callback(); });
  }
}

//==============================================================================
// Predefined sequences

ChordSequenceDetector::UnlockSequence ChordSequenceDetector::createBasicSynthSequence(std::function<void()> onUnlock) {
  UnlockSequence seq;
  seq.name = "basic_synth";
  seq.onUnlock = std::move(onUnlock);

  // C major chord (just one chord to start simple)
  // C-E-G (0, 4, 7)
  Chord cMajor;
  cMajor.rootNote = 0; // C
  cMajor.type = ChordType::Major;
  seq.chords.push_back(cMajor);

  // E minor
  Chord eMinor;
  eMinor.rootNote = 4; // E
  eMinor.type = ChordType::Minor;
  seq.chords.push_back(eMinor);

  // G major
  Chord gMajor;
  gMajor.rootNote = 7; // G
  gMajor.type = ChordType::Major;
  seq.chords.push_back(gMajor);

  return seq;
}

ChordSequenceDetector::UnlockSequence
ChordSequenceDetector::createAdvancedSynthSequence(std::function<void()> onUnlock) {
  UnlockSequence seq;
  seq.name = "advanced_synth";
  seq.onUnlock = std::move(onUnlock);

  // Am - F - C - G (vi - IV - I - V in C major)
  Chord am;
  am.rootNote = 9; // A
  am.type = ChordType::Minor;
  seq.chords.push_back(am);

  Chord f;
  f.rootNote = 5; // F
  f.type = ChordType::Major;
  seq.chords.push_back(f);

  Chord c;
  c.rootNote = 0; // C
  c.type = ChordType::Major;
  seq.chords.push_back(c);

  Chord g;
  g.rootNote = 7; // G
  g.type = ChordType::Major;
  seq.chords.push_back(g);

  return seq;
}

ChordSequenceDetector::UnlockSequence ChordSequenceDetector::createSecretSequence(std::function<void()> onUnlock) {
  UnlockSequence seq;
  seq.name = "secret_synth";
  seq.onUnlock = std::move(onUnlock);

  // Dm - G - C - Am (ii - V - I - vi)
  Chord dm;
  dm.rootNote = 2; // D
  dm.type = ChordType::Minor;
  seq.chords.push_back(dm);

  Chord g;
  g.rootNote = 7; // G
  g.type = ChordType::Major;
  seq.chords.push_back(g);

  Chord c;
  c.rootNote = 0; // C
  c.type = ChordType::Major;
  seq.chords.push_back(c);

  Chord am;
  am.rootNote = 9; // A
  am.type = ChordType::Minor;
  seq.chords.push_back(am);

  return seq;
}
