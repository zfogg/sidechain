#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>

// ==============================================================================
/**
 * SynthEngine - A simple polyphonic synthesizer for the easter egg feature
 *
 * Features:
 * - Multiple oscillator waveforms (sine, saw, square, triangle)
 * - Low-pass filter with resonance
 * - ADSR envelope
 * - 8-voice polyphony
 * - Preset system
 */
class SynthEngine {
public:
  // ==============================================================================
  /** Oscillator waveform types */
  enum class Waveform { Sine, Saw, Square, Triangle };

  /** Synth preset */
  struct Preset {
    juce::String name;
    Waveform waveform = Waveform::Saw;
    float attack = 0.01f;         // seconds
    float decay = 0.1f;           // seconds
    float sustain = 0.7f;         // 0-1
    float release = 0.3f;         // seconds
    float filterCutoff = 2000.0f; // Hz
    float filterResonance = 0.5f; // 0-1
    float filterEnvAmount = 0.0f; // 0-1
    float detuneAmount = 0.0f;    // cents
    float volume = 0.7f;          // 0-1
  };

  // ==============================================================================
  /** Constructor */
  SynthEngine();

  /** Destructor */
  ~SynthEngine() = default;

  // ==============================================================================
  // Audio Processing

  /** Prepare the synth for playback
   *  @param sampleRate The sample rate
   *  @param samplesPerBlock Maximum block size
   */
  void prepare(double sampleRate, int samplesPerBlock);

  /** Process audio block
   *  @param buffer Output audio buffer (will be added to, not replaced)
   *  @param midiMessages MIDI messages for this block
   */
  void process(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midiMessages);

  /** Reset all voices and state */
  void reset();

  // ==============================================================================
  // MIDI Control

  /** Handle note on
   *  @param noteNumber MIDI note number
   *  @param velocity Note velocity (0-127)
   */
  void noteOn(int noteNumber, int velocity);

  /** Handle note off
   *  @param noteNumber MIDI note number
   */
  void noteOff(int noteNumber);

  /** All notes off */
  void allNotesOff();

  // ==============================================================================
  // Parameters

  /** Set oscillator waveform */
  void setWaveform(Waveform waveform) {
    currentWaveform.store(static_cast<int>(waveform));
  }

  /** Get current waveform */
  Waveform getWaveform() const {
    return static_cast<Waveform>(currentWaveform.load());
  }

  /** Set ADSR envelope parameters */
  void setADSR(float attack, float decay, float sustain, float release);

  /** Set filter cutoff frequency (Hz) */
  void setFilterCutoff(float cutoff) {
    filterCutoff.store(cutoff);
  }

  /** Get filter cutoff */
  float getFilterCutoff() const {
    return filterCutoff.load();
  }

  /** Set filter resonance (0-1) */
  void setFilterResonance(float resonance) {
    filterResonance.store(resonance);
  }

  /** Get filter resonance */
  float getFilterResonance() const {
    return filterResonance.load();
  }

  /** Set master volume (0-1) */
  void setVolume(float vol) {
    volume.store(vol);
  }

  /** Get master volume */
  float getVolume() const {
    return volume.load();
  }

  /** Set detune amount in cents */
  void setDetune(float cents) {
    detuneAmount.store(cents);
  }

  /** Get detune amount */
  float getDetune() const {
    return detuneAmount.load();
  }

  // ==============================================================================
  // Presets

  /** Load a preset */
  void loadPreset(const Preset &preset);

  /** Get current preset settings as a Preset object */
  Preset getCurrentPreset() const;

  /** Get default presets */
  static std::vector<Preset> getDefaultPresets();

  // ==============================================================================
  // State

  /** Check if synth is currently producing sound */
  bool isPlaying() const;

  /** Get number of active voices */
  int getActiveVoiceCount() const;

private:
  // ==============================================================================
  /** Single voice for polyphony */
  struct Voice {
    bool active = false;
    int noteNumber = -1;
    float velocity = 0.0f;
    float phase = 0.0f;
    float phaseIncrement = 0.0f;

    // Envelope state
    enum class EnvStage { Off, Attack, Decay, Sustain, Release };
    EnvStage envStage = EnvStage::Off;
    float envValue = 0.0f;
    float releaseStartValue = 0.0f;
    int envSampleCount = 0;

    // Filter state (per-voice for better sound)
    float filterState1 = 0.0f;
    float filterState2 = 0.0f;
  };

  // ==============================================================================
  // Voice management
  static constexpr int numVoices = 8;
  std::array<Voice, numVoices> voices;

  Voice *findFreeVoice();
  Voice *findVoiceForNote(int noteNumber);
  void startVoice(Voice &voice, int noteNumber, float velocity);
  void stopVoice(Voice &voice);

  // ==============================================================================
  // Oscillator generation
  float generateSample(Voice &voice, Waveform waveform);
  static float midiNoteToFrequency(int noteNumber, float detuneCents = 0.0f);

  // ==============================================================================
  // Filter processing
  float processFilter(Voice &voice, float input, float cutoff, float resonance);

  // ==============================================================================
  // Envelope processing
  float processEnvelope(Voice &voice);

  // ==============================================================================
  // Audio settings
  double sampleRate = 44100.0;
  int blockSize = 512;

  // Parameters (atomic for thread safety)
  std::atomic<int> currentWaveform{static_cast<int>(Waveform::Saw)};
  std::atomic<float> attackTime{0.01f};
  std::atomic<float> decayTime{0.1f};
  std::atomic<float> sustainLevel{0.7f};
  std::atomic<float> releaseTime{0.3f};
  std::atomic<float> filterCutoff{2000.0f};
  std::atomic<float> filterResonance{0.5f};
  std::atomic<float> volume{0.7f};
  std::atomic<float> detuneAmount{0.0f};

  // Critical section for voice access
  juce::CriticalSection voiceLock;
};
