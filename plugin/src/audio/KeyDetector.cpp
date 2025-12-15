#include "KeyDetector.h"

#if SIDECHAIN_HAS_KEYFINDER
#include <audiodata.h>
#include <constants.h>
#include <keyfinder.h>
#endif

//==============================================================================
// Key string lookup tables
//==============================================================================

namespace KeyNames {
// Standard key names (index matches libkeyfinder's key_t enum)
static const char *const standardNames[] = {
    "A major",  // 0  - A_MAJOR
    "A minor",  // 1  - A_MINOR
    "Bb major", // 2  - B_FLAT_MAJOR
    "Bb minor", // 3  - B_FLAT_MINOR
    "B major",  // 4  - B_MAJOR
    "B minor",  // 5  - B_MINOR
    "C major",  // 6  - C_MAJOR
    "C minor",  // 7  - C_MINOR
    "Db major", // 8  - D_FLAT_MAJOR
    "Db minor", // 9  - D_FLAT_MINOR
    "D major",  // 10 - D_MAJOR
    "D minor",  // 11 - D_MINOR
    "Eb major", // 12 - E_FLAT_MAJOR
    "Eb minor", // 13 - E_FLAT_MINOR
    "E major",  // 14 - E_MAJOR
    "E minor",  // 15 - E_MINOR
    "F major",  // 16 - F_MAJOR
    "F minor",  // 17 - F_MINOR
    "F# major", // 18 - G_FLAT_MAJOR (enharmonic)
    "F# minor", // 19 - G_FLAT_MINOR (enharmonic)
    "G major",  // 20 - G_MAJOR
    "G minor",  // 21 - G_MINOR
    "Ab major", // 22 - A_FLAT_MAJOR
    "Ab minor", // 23 - A_FLAT_MINOR
    "Silence"   // 24 - SILENCE
};

// Short names (Am, F#, etc.)
static const char *const shortNames[] = {
    "A",  "Am",  "Bb", "Bbm", "B", "Bm", "C",  "Cm",  "Db", "Dbm", "D",  "Dm",
    "Eb", "Ebm", "E",  "Em",  "F", "Fm", "F#", "F#m", "G",  "Gm",  "Ab", "Abm",
    "" // Silence
};

// Camelot wheel notation
// Major keys: 1B-12B, Minor keys: 1A-12A
static const char *const camelotNames[] = {
    "11B", "8A",  // A major, A minor
    "6B",  "3A",  // Bb major, Bb minor
    "1B",  "10A", // B major, B minor
    "8B",  "5A",  // C major, C minor
    "3B",  "12A", // Db major, Db minor
    "10B", "7A",  // D major, D minor
    "5B",  "2A",  // Eb major, Eb minor
    "12B", "9A",  // E major, E minor
    "7B",  "4A",  // F major, F minor
    "2B",  "11A", // F# major, F# minor
    "9B",  "6A",  // G major, G minor
    "4B",  "1A",  // Ab major, Ab minor
    ""            // Silence
};

// Root note values (0-11, C=0)
static const int rootNotes[] = {
    9,  9,  // A
    10, 10, // Bb
    11, 11, // B
    0,  0,  // C
    1,  1,  // Db
    2,  2,  // D
    3,  3,  // Eb
    4,  4,  // E
    5,  5,  // F
    6,  6,  // F#
    7,  7,  // G
    8,  8,  // Ab
    -1      // Silence
};
} // namespace KeyNames

//==============================================================================
// KeyDetector::Key implementation
//==============================================================================

KeyDetector::Key KeyDetector::Key::fromString(const juce::String &keyStr) {
  Key key;
  auto trimmed = keyStr.trim().toLowerCase();

  // Try to match against known key names
  for (int i = 0; i < 24; ++i) {
    if (trimmed == juce::String(KeyNames::standardNames[i]).toLowerCase() ||
        trimmed == juce::String(KeyNames::shortNames[i]).toLowerCase() ||
        trimmed == juce::String(KeyNames::camelotNames[i]).toLowerCase()) {
      key.name = KeyNames::standardNames[i];
      key.shortName = KeyNames::shortNames[i];
      key.camelot = KeyNames::camelotNames[i];
      key.isMajor = (i % 2 == 0); // Even indices are major
      key.rootNote = KeyNames::rootNotes[i];
      key.confidence = 1.0f; // Manual selection = full confidence
      return key;
    }
  }

  return key; // Invalid key
}

juce::StringArray KeyDetector::Key::getAllKeys() {
  juce::StringArray keys;
  for (int i = 0; i < 24; ++i)
    keys.add(KeyNames::standardNames[i]);
  return keys;
}

juce::StringArray KeyDetector::Key::getAllCamelotKeys() {
  juce::StringArray keys;
  for (int i = 0; i < 24; ++i)
    keys.add(KeyNames::camelotNames[i]);
  return keys;
}

//==============================================================================
// KeyDetector Implementation (with libkeyfinder)
//==============================================================================

#if SIDECHAIN_HAS_KEYFINDER

class KeyDetector::Impl {
public:
  Impl() = default;

  KeyDetector::Key detectKey(const juce::AudioBuffer<float> &buffer, double sampleRate, int numChannels) {
    Key result;

    if (buffer.getNumSamples() == 0)
      return result;

    try {
      // Create KeyFinder AudioData
      KeyFinder::AudioData audioData;
      audioData.setFrameRate(static_cast<unsigned int>(sampleRate));
      audioData.setChannels(1); // Mix to mono

      // Calculate number of samples
      int numSamples = buffer.getNumSamples();
      audioData.addToSampleCount(numSamples);

      // Mix to mono and copy to AudioData
      for (int i = 0; i < numSamples; ++i) {
        float sample = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch) {
          sample += buffer.getSample(ch, i);
        }
        sample /= static_cast<float>(numChannels);

        // libkeyfinder expects samples in range [-1, 1]
        audioData.setSample(i, static_cast<double>(sample));
      }

      // Run key detection
      KeyFinder::KeyFinder keyFinder;
      KeyFinder::key_t detectedKey = keyFinder.keyOfAudio(audioData);

      // Convert to our Key struct
      int keyIndex = static_cast<int>(detectedKey);
      if (keyIndex >= 0 && keyIndex < 24) {
        result.name = KeyNames::standardNames[keyIndex];
        result.shortName = KeyNames::shortNames[keyIndex];
        result.camelot = KeyNames::camelotNames[keyIndex];
        result.isMajor = (keyIndex % 2 == 0);
        result.rootNote = KeyNames::rootNotes[keyIndex];
        result.confidence = 0.8f; // libkeyfinder doesn't provide confidence
      }
    } catch (const std::exception &e) {
      DBG("Key detection failed: " << e.what());
    }

    return result;
  }
};

bool KeyDetector::isAvailable() {
  return true;
}

#else

// Stub implementation when libkeyfinder is not available
class KeyDetector::Impl {
public:
  KeyDetector::Key detectKey(const juce::AudioBuffer<float> &, double, int) {
    return Key(); // Return invalid key
  }
};

bool KeyDetector::isAvailable() {
  return false;
}

#endif

//==============================================================================
// KeyDetector public methods
//==============================================================================

KeyDetector::KeyDetector() : impl(std::make_unique<Impl>()) {}

KeyDetector::~KeyDetector() = default;

KeyDetector::Key KeyDetector::detectKey(const juce::AudioBuffer<float> &buffer, double sampleRate, int numChannels) {
  return impl->detectKey(buffer, sampleRate, numChannels);
}

KeyDetector::Key KeyDetector::detectKeyFromFile(const juce::File &audioFile) {
  Key result;

  juce::AudioFormatManager formatManager;
  formatManager.registerBasicFormats();

  std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(audioFile));

  if (reader == nullptr) {
    DBG("Failed to read audio file: " << audioFile.getFullPathName());
    return result;
  }

  // Read entire file into buffer
  juce::AudioBuffer<float> buffer(static_cast<int>(reader->numChannels), static_cast<int>(reader->lengthInSamples));

  reader->read(&buffer, 0, static_cast<int>(reader->lengthInSamples), 0, true, true);

  return detectKey(buffer, reader->sampleRate, static_cast<int>(reader->numChannels));
}

juce::String KeyDetector::keyToString(int keyValue) {
  if (keyValue >= 0 && keyValue < 25)
    return KeyNames::standardNames[keyValue];
  return "Unknown";
}

juce::String KeyDetector::keyToCamelot(int keyValue) {
  if (keyValue >= 0 && keyValue < 25)
    return KeyNames::camelotNames[keyValue];
  return "";
}

juce::String KeyDetector::getRootNoteName(int keyValue) {
  if (keyValue >= 0 && keyValue < 24) {
    static const char *const noteNames[] = {"C", "Db", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"};
    int rootNote = KeyNames::rootNotes[keyValue];
    if (rootNote >= 0 && rootNote < 12)
      return noteNames[rootNote];
  }
  return "";
}

bool KeyDetector::isMajorKey(int keyValue) {
  return (keyValue >= 0 && keyValue < 24) ? (keyValue % 2 == 0) : false;
}
