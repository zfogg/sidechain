#include "ProgressiveKeyDetector.h"

#if SIDECHAIN_HAS_KEYFINDER
#include <audiodata.h>
#include <keyfinder.h>
#include <workspace.h>
#endif

// ==============================================================================
// Key string lookup tables (shared with KeyDetector)
// ==============================================================================

namespace KeyNames {
// Standard key names (index matches libkeyfinder's key_t enum)
static const char *const standardNames[] = {
    "A major",  // 0 - A_MAJOR
    "A minor",  // 1 - A_MINOR
    "Bb major", // 2 - B_FLAT_MAJOR
    "Bb minor", // 3 - B_FLAT_MINOR
    "B major",  // 4 - B_MAJOR
    "B minor",  // 5 - B_MINOR
    "C major",  // 6 - C_MAJOR
    "C minor",  // 7 - C_MINOR
    "Db major", // 8 - D_FLAT_MAJOR
    "Db minor", // 9 - D_FLAT_MINOR
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
    "A",  "Am",  "Bb", "Bbm", "B", "Bm", "C",   "Cm",  "Db", "Dbm", "D",  "Dm",
    "Eb", "Ebm", "E",  "Em",  "F", "Fm", "F# ", "F#m", "G",  "Gm",  "Ab", "Abm",
    "" // Silence
};

// Camelot wheel notation
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

// ==============================================================================
// Helper function to convert libkeyfinder key_t to KeyDetector::Key
// ==============================================================================

#if SIDECHAIN_HAS_KEYFINDER
static KeyDetector::Key convertKey(KeyFinder::key_t detectedKey) {
  KeyDetector::Key result;
  int keyIndex = static_cast<int>(detectedKey);

  if (keyIndex >= 0 && keyIndex < 24) {
    result.name = KeyNames::standardNames[keyIndex];
    result.shortName = KeyNames::shortNames[keyIndex];
    result.camelot = KeyNames::camelotNames[keyIndex];
    result.isMajor = (keyIndex % 2 == 0);
    result.rootNote = KeyNames::rootNotes[keyIndex];
    result.confidence = 0.8f; // libkeyfinder doesn't provide confidence
  }

  return result;
}
#endif

// ==============================================================================
// ProgressiveKeyDetector Implementation (with libkeyfinder)
// ==============================================================================

#if SIDECHAIN_HAS_KEYFINDER

class ProgressiveKeyDetector::Impl {
public:
  Impl() = default;

  bool start(double sampleRateParam) {
    try {
      workspace = std::make_unique<KeyFinder::Workspace>();
      keyFinder = std::make_unique<KeyFinder::KeyFinder>();
      this->sampleRate = sampleRateParam;
      return true;
    } catch (const std::exception &e) {
      DBG("ProgressiveKeyDetector::start failed: " << e.what());
      return false;
    }
  }

  bool addAudioChunk(const juce::AudioBuffer<float> &buffer, int numChannels) {
    if (!workspace || !keyFinder)
      return false;

    if (buffer.getNumSamples() == 0)
      return true; // Empty buffer is fine

    try {
      // Create KeyFinder AudioData for this chunk
      KeyFinder::AudioData audioData;
      audioData.setFrameRate(static_cast<unsigned int>(sampleRate));
      audioData.setChannels(1); // Mix to mono

      int numSamples = buffer.getNumSamples();
      audioData.addToSampleCount(static_cast<unsigned int>(numSamples));

      // Mix to mono and copy to AudioData
      for (int i = 0; i < numSamples; ++i) {
        float sample = 0.0f;
        for (int ch = 0; ch < numChannels && ch < buffer.getNumChannels(); ++ch) {
          sample += buffer.getSample(ch, i);
        }
        sample /= static_cast<float>(numChannels);

        // libkeyfinder expects samples in range [-1, 1]
        audioData.setSample(static_cast<unsigned int>(i), static_cast<double>(sample));
      }

      // Process this chunk progressively
      keyFinder->progressiveChromagram(audioData, *workspace);

      return true;
    } catch (const std::exception &e) {
      DBG("ProgressiveKeyDetector::addAudioChunk failed: " << e.what());
      return false;
    }
  }

  KeyDetector::Key getCurrentKey() const {
    KeyDetector::Key result;

    if (!workspace || !keyFinder)
      return result;

    try {
      // Get current key estimate from chromagram (without finalizing)
      KeyFinder::key_t detectedKey = keyFinder->keyOfChromagram(*workspace);
      return convertKey(detectedKey);
    } catch (const std::exception &e) {
      DBG("ProgressiveKeyDetector::getCurrentKey failed: " << e.what());
      return result;
    }
  }

  bool finalize() {
    if (!workspace || !keyFinder)
      return false;

    try {
      keyFinder->finalChromagram(*workspace);
      return true;
    } catch (const std::exception &e) {
      DBG("ProgressiveKeyDetector::finalize failed: " << e.what());
      return false;
    }
  }

  KeyDetector::Key getFinalKey() const {
    return getCurrentKey(); // After finalize, getCurrentKey returns final
                            // result
  }

  void reset() {
    workspace.reset();
    keyFinder.reset();
  }

private:
  std::unique_ptr<KeyFinder::Workspace> workspace;
  std::unique_ptr<KeyFinder::KeyFinder> keyFinder;
  double sampleRate = 0.0;
};

bool ProgressiveKeyDetector::isAvailable() {
  return true;
}

#else

// Stub implementation when libkeyfinder is not available
class ProgressiveKeyDetector::Impl {
public:
  bool start(double) {
    return false;
  }
  bool addAudioChunk(const juce::AudioBuffer<float> &, int) {
    return false;
  }
  KeyDetector::Key getCurrentKey() const {
    return KeyDetector::Key();
  }
  bool finalize() {
    return false;
  }
  KeyDetector::Key getFinalKey() const {
    return KeyDetector::Key();
  }
  void reset() {}
};

bool ProgressiveKeyDetector::isAvailable() {
  return false;
}

#endif

// ==============================================================================
// ProgressiveKeyDetector public methods (outside conditional compilation)
// ==============================================================================

// Note: Constructor and destructor are defined above in the conditional blocks
// but we need to ensure they're always available

// ==============================================================================
// ProgressiveKeyDetector public methods
// ==============================================================================

ProgressiveKeyDetector::ProgressiveKeyDetector() : impl(std::make_unique<Impl>()) {}

ProgressiveKeyDetector::~ProgressiveKeyDetector() = default;

bool ProgressiveKeyDetector::start(double sampleRateParam) {
  if (!isAvailable())
    return false;

  reset(); // Clear any previous state

#if SIDECHAIN_HAS_KEYFINDER
  if (impl->start(sampleRateParam)) {
    this->sampleRate = sampleRateParam;
    this->active = true;
    this->finalized = false;
    this->samplesProcessed = 0;
    return true;
  }
#endif

  return false;
}

bool ProgressiveKeyDetector::addAudioChunk(const juce::AudioBuffer<float> &buffer, int numChannels) {
  if (!active || finalized)
    return false;

#if SIDECHAIN_HAS_KEYFINDER
  if (impl->addAudioChunk(buffer, numChannels)) {
    samplesProcessed += buffer.getNumSamples();
    return true;
  }
#endif

  return false;
}

KeyDetector::Key ProgressiveKeyDetector::getCurrentKey() const {
  if (!active)
    return KeyDetector::Key();

#if SIDECHAIN_HAS_KEYFINDER
  return impl->getCurrentKey();
#else
  return KeyDetector::Key();
#endif
}

bool ProgressiveKeyDetector::finalize() {
  if (!active || finalized)
    return false;

#if SIDECHAIN_HAS_KEYFINDER
  if (impl->finalize()) {
    finalized = true;
    return true;
  }
#endif

  return false;
}

KeyDetector::Key ProgressiveKeyDetector::getFinalKey() const {
  if (!active || !finalized)
    return KeyDetector::Key();

#if SIDECHAIN_HAS_KEYFINDER
  return impl->getFinalKey();
#else
  return KeyDetector::Key();
#endif
}

void ProgressiveKeyDetector::reset() {
#if SIDECHAIN_HAS_KEYFINDER
  if (impl)
    impl->reset();
#endif

  active = false;
  finalized = false;
  sampleRate = 0.0;
  samplesProcessed = 0;
}
