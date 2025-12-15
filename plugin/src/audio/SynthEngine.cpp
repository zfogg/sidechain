#include "SynthEngine.h"
#include <cmath>

//==============================================================================
SynthEngine::SynthEngine() {}

//==============================================================================
void SynthEngine::prepare(double newSampleRate, int samplesPerBlock) {
  sampleRate = newSampleRate;
  blockSize = samplesPerBlock;
  reset();
}

void SynthEngine::process(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midiMessages) {
  // Process MIDI messages
  for (const auto metadata : midiMessages) {
    const auto msg = metadata.getMessage();

    if (msg.isNoteOn()) {
      noteOn(msg.getNoteNumber(), msg.getVelocity());
    } else if (msg.isNoteOff()) {
      noteOff(msg.getNoteNumber());
    } else if (msg.isAllNotesOff() || msg.isAllSoundOff()) {
      allNotesOff();
    }
  }

  // Generate audio
  const int numSamples = buffer.getNumSamples();
  const int numChannels = buffer.getNumChannels();
  const float vol = volume.load();
  const Waveform waveform = static_cast<Waveform>(currentWaveform.load());
  const float cutoff = filterCutoff.load();
  const float resonance = filterResonance.load();

  juce::ScopedLock lock(voiceLock);

  for (int sample = 0; sample < numSamples; ++sample) {
    float mixedSample = 0.0f;

    for (auto &voice : voices) {
      if (!voice.active)
        continue;

      // Generate oscillator
      float osc = generateSample(voice, waveform);

      // Apply envelope
      float env = processEnvelope(voice);

      // Apply filter
      float filtered = processFilter(voice, osc, cutoff, resonance);

      // Mix
      mixedSample += filtered * env * voice.velocity * vol;
    }

    // Apply soft clipping
    mixedSample = std::tanh(mixedSample);

    // Write to all channels
    for (int channel = 0; channel < numChannels; ++channel) {
      buffer.addSample(channel, sample, mixedSample);
    }
  }
}

void SynthEngine::reset() {
  juce::ScopedLock lock(voiceLock);

  for (auto &voice : voices) {
    voice.active = false;
    voice.noteNumber = -1;
    voice.envStage = Voice::EnvStage::Off;
    voice.envValue = 0.0f;
    voice.phase = 0.0f;
    voice.filterState1 = 0.0f;
    voice.filterState2 = 0.0f;
  }
}

//==============================================================================
void SynthEngine::noteOn(int noteNumber, int velocity) {
  juce::ScopedLock lock(voiceLock);

  // Check if note is already playing, retrigger if so
  Voice *voice = findVoiceForNote(noteNumber);
  if (voice == nullptr) {
    voice = findFreeVoice();
  }

  if (voice != nullptr) {
    startVoice(*voice, noteNumber, velocity / 127.0f);
  }
}

void SynthEngine::noteOff(int noteNumber) {
  juce::ScopedLock lock(voiceLock);

  Voice *voice = findVoiceForNote(noteNumber);
  if (voice != nullptr) {
    stopVoice(*voice);
  }
}

void SynthEngine::allNotesOff() {
  juce::ScopedLock lock(voiceLock);

  for (auto &voice : voices) {
    if (voice.active) {
      stopVoice(voice);
    }
  }
}

//==============================================================================
void SynthEngine::setADSR(float attack, float decay, float sustain, float release) {
  attackTime.store(std::max(0.001f, attack));
  decayTime.store(std::max(0.001f, decay));
  sustainLevel.store(juce::jlimit(0.0f, 1.0f, sustain));
  releaseTime.store(std::max(0.001f, release));
}

//==============================================================================
void SynthEngine::loadPreset(const Preset &preset) {
  setWaveform(preset.waveform);
  setADSR(preset.attack, preset.decay, preset.sustain, preset.release);
  setFilterCutoff(preset.filterCutoff);
  setFilterResonance(preset.filterResonance);
  setVolume(preset.volume);
  setDetune(preset.detuneAmount);
}

SynthEngine::Preset SynthEngine::getCurrentPreset() const {
  Preset preset;
  preset.waveform = getWaveform();
  preset.attack = attackTime.load();
  preset.decay = decayTime.load();
  preset.sustain = sustainLevel.load();
  preset.release = releaseTime.load();
  preset.filterCutoff = getFilterCutoff();
  preset.filterResonance = getFilterResonance();
  preset.volume = getVolume();
  preset.detuneAmount = getDetune();
  return preset;
}

std::vector<SynthEngine::Preset> SynthEngine::getDefaultPresets() {
  std::vector<Preset> presets;

  // Init / Basic
  {
    Preset p;
    p.name = "Init";
    p.waveform = Waveform::Saw;
    p.attack = 0.01f;
    p.decay = 0.1f;
    p.sustain = 0.7f;
    p.release = 0.3f;
    p.filterCutoff = 2000.0f;
    p.filterResonance = 0.3f;
    p.volume = 0.7f;
    presets.push_back(p);
  }

  // Pad
  {
    Preset p;
    p.name = "Soft Pad";
    p.waveform = Waveform::Sine;
    p.attack = 0.5f;
    p.decay = 0.3f;
    p.sustain = 0.8f;
    p.release = 1.0f;
    p.filterCutoff = 1500.0f;
    p.filterResonance = 0.2f;
    p.volume = 0.6f;
    presets.push_back(p);
  }

  // Bass
  {
    Preset p;
    p.name = "Sub Bass";
    p.waveform = Waveform::Sine;
    p.attack = 0.005f;
    p.decay = 0.2f;
    p.sustain = 0.6f;
    p.release = 0.15f;
    p.filterCutoff = 500.0f;
    p.filterResonance = 0.4f;
    p.volume = 0.8f;
    presets.push_back(p);
  }

  // Lead
  {
    Preset p;
    p.name = "Saw Lead";
    p.waveform = Waveform::Saw;
    p.attack = 0.01f;
    p.decay = 0.15f;
    p.sustain = 0.5f;
    p.release = 0.2f;
    p.filterCutoff = 3000.0f;
    p.filterResonance = 0.5f;
    p.volume = 0.7f;
    p.detuneAmount = 10.0f;
    presets.push_back(p);
  }

  // Square Lead
  {
    Preset p;
    p.name = "Square Lead";
    p.waveform = Waveform::Square;
    p.attack = 0.01f;
    p.decay = 0.1f;
    p.sustain = 0.6f;
    p.release = 0.25f;
    p.filterCutoff = 2500.0f;
    p.filterResonance = 0.4f;
    p.volume = 0.65f;
    presets.push_back(p);
  }

  // Pluck
  {
    Preset p;
    p.name = "Pluck";
    p.waveform = Waveform::Triangle;
    p.attack = 0.001f;
    p.decay = 0.3f;
    p.sustain = 0.0f;
    p.release = 0.2f;
    p.filterCutoff = 4000.0f;
    p.filterResonance = 0.6f;
    p.volume = 0.75f;
    presets.push_back(p);
  }

  // Brass
  {
    Preset p;
    p.name = "Brass";
    p.waveform = Waveform::Saw;
    p.attack = 0.08f;
    p.decay = 0.2f;
    p.sustain = 0.7f;
    p.release = 0.15f;
    p.filterCutoff = 1800.0f;
    p.filterResonance = 0.35f;
    p.volume = 0.7f;
    presets.push_back(p);
  }

  // Retro
  {
    Preset p;
    p.name = "Retro";
    p.waveform = Waveform::Square;
    p.attack = 0.005f;
    p.decay = 0.1f;
    p.sustain = 0.4f;
    p.release = 0.1f;
    p.filterCutoff = 1200.0f;
    p.filterResonance = 0.7f;
    p.volume = 0.6f;
    presets.push_back(p);
  }

  return presets;
}

//==============================================================================
bool SynthEngine::isPlaying() const {
  juce::ScopedLock lock(voiceLock);

  for (const auto &voice : voices) {
    if (voice.active)
      return true;
  }
  return false;
}

int SynthEngine::getActiveVoiceCount() const {
  juce::ScopedLock lock(voiceLock);

  int count = 0;
  for (const auto &voice : voices) {
    if (voice.active)
      count++;
  }
  return count;
}

//==============================================================================
SynthEngine::Voice *SynthEngine::findFreeVoice() {
  // First, try to find a completely inactive voice
  for (auto &voice : voices) {
    if (!voice.active)
      return &voice;
  }

  // If all voices are active, steal the oldest one in release stage
  Voice *oldest = nullptr;
  int oldestSampleCount = 0;

  for (auto &voice : voices) {
    if (voice.envStage == Voice::EnvStage::Release) {
      if (oldest == nullptr || voice.envSampleCount > oldestSampleCount) {
        oldest = &voice;
        oldestSampleCount = voice.envSampleCount;
      }
    }
  }

  if (oldest != nullptr)
    return oldest;

  // Last resort: steal any voice
  return &voices[0];
}

SynthEngine::Voice *SynthEngine::findVoiceForNote(int noteNumber) {
  for (auto &voice : voices) {
    if (voice.active && voice.noteNumber == noteNumber)
      return &voice;
  }
  return nullptr;
}

void SynthEngine::startVoice(Voice &voice, int noteNumber, float velocity) {
  voice.active = true;
  voice.noteNumber = noteNumber;
  voice.velocity = velocity;
  voice.phase = 0.0f;
  voice.phaseIncrement = midiNoteToFrequency(noteNumber, detuneAmount.load()) / static_cast<float>(sampleRate);
  voice.envStage = Voice::EnvStage::Attack;
  voice.envValue = 0.0f;
  voice.envSampleCount = 0;
  voice.filterState1 = 0.0f;
  voice.filterState2 = 0.0f;
}

void SynthEngine::stopVoice(Voice &voice) {
  if (voice.envStage != Voice::EnvStage::Off) {
    voice.envStage = Voice::EnvStage::Release;
    voice.releaseStartValue = voice.envValue;
    voice.envSampleCount = 0;
  }
}

//==============================================================================
float SynthEngine::generateSample(Voice &voice, Waveform waveform) {
  float sample = 0.0f;

  switch (waveform) {
  case Waveform::Sine:
    sample = std::sin(voice.phase * juce::MathConstants<float>::twoPi);
    break;

  case Waveform::Saw:
    sample = 2.0f * voice.phase - 1.0f;
    break;

  case Waveform::Square:
    sample = voice.phase < 0.5f ? 1.0f : -1.0f;
    break;

  case Waveform::Triangle:
    sample = 4.0f * std::abs(voice.phase - 0.5f) - 1.0f;
    break;
  }

  // Advance phase
  voice.phase += voice.phaseIncrement;
  if (voice.phase >= 1.0f)
    voice.phase -= 1.0f;

  return sample;
}

float SynthEngine::midiNoteToFrequency(int noteNumber, float detuneCents) {
  // A4 = 440 Hz = MIDI note 69
  float semitones = static_cast<float>(noteNumber - 69) + detuneCents / 100.0f;
  return 440.0f * std::pow(2.0f, semitones / 12.0f);
}

//==============================================================================
float SynthEngine::processFilter(Voice &voice, float input, float cutoff, float resonance) {
  // Simple 2-pole low-pass filter (SVF style)
  const float nyquist = static_cast<float>(sampleRate) * 0.5f;
  const float normalizedCutoff = std::min(cutoff / nyquist, 0.99f);

  const float f = 2.0f * std::sin(juce::MathConstants<float>::pi * normalizedCutoff * 0.5f);
  const float q = 1.0f - resonance * 0.9f; // Prevent self-oscillation

  // State variable filter
  const float hp = input - voice.filterState1 - q * voice.filterState2;
  const float bp = hp * f + voice.filterState2;
  const float lp = bp * f + voice.filterState1;

  voice.filterState1 = lp;
  voice.filterState2 = bp;

  return lp;
}

//==============================================================================
float SynthEngine::processEnvelope(Voice &voice) {
  const float attack = attackTime.load();
  const float decay = decayTime.load();
  const float sustain = sustainLevel.load();
  const float release = releaseTime.load();

  const float attackSamples = attack * static_cast<float>(sampleRate);
  const float decaySamples = decay * static_cast<float>(sampleRate);
  const float releaseSamples = release * static_cast<float>(sampleRate);

  switch (voice.envStage) {
  case Voice::EnvStage::Attack:
    voice.envValue = static_cast<float>(voice.envSampleCount) / attackSamples;
    if (voice.envValue >= 1.0f) {
      voice.envValue = 1.0f;
      voice.envStage = Voice::EnvStage::Decay;
      voice.envSampleCount = 0;
    }
    break;

  case Voice::EnvStage::Decay: {
    float decayProgress = static_cast<float>(voice.envSampleCount) / decaySamples;
    voice.envValue = 1.0f - (1.0f - sustain) * decayProgress;
    if (decayProgress >= 1.0f) {
      voice.envValue = sustain;
      voice.envStage = Voice::EnvStage::Sustain;
    }
    break;
  }

  case Voice::EnvStage::Sustain:
    voice.envValue = sustain;
    break;

  case Voice::EnvStage::Release: {
    float releaseProgress = static_cast<float>(voice.envSampleCount) / releaseSamples;
    voice.envValue = voice.releaseStartValue * (1.0f - releaseProgress);
    if (releaseProgress >= 1.0f) {
      voice.envValue = 0.0f;
      voice.envStage = Voice::EnvStage::Off;
      voice.active = false;
    }
    break;
  }

  case Voice::EnvStage::Off:
    voice.envValue = 0.0f;
    break;
  }

  voice.envSampleCount++;
  return voice.envValue;
}
