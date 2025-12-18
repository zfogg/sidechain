#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "audio/BufferAudioPlayer.h"
#include "network/NetworkClient.h"
#include "stores/AppStore.h"
#include "util/Log.h"
#include "util/profiling/PerformanceMonitor.h"

//==============================================================================
SidechainAudioProcessor::SidechainAudioProcessor()
    : AudioProcessor(BusesProperties()
#if !JucePlugin_IsMidiEffect
#if !JucePlugin_IsSynth
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
#endif
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)
#endif
      ) {
  // Setup chord sequence detector unlock callbacks (R.2.1)
  chordDetector.addUnlockSequence(ChordSequenceDetector::createBasicSynthSequence([this]() {
    Log::info("SidechainAudioProcessor: Basic synth unlocked!");
    synthUnlocked.store(true);
    if (onSynthUnlocked)
      onSynthUnlocked();
  }));

  chordDetector.addUnlockSequence(ChordSequenceDetector::createAdvancedSynthSequence([this]() {
    Log::info("SidechainAudioProcessor: Advanced synth unlocked!");
    synthUnlocked.store(true);
    if (onSynthUnlocked)
      onSynthUnlocked();
  }));

  chordDetector.addUnlockSequence(ChordSequenceDetector::createSecretSequence([this]() {
    Log::info("SidechainAudioProcessor: Secret synth unlocked!");
    synthUnlocked.store(true);
    if (onSynthUnlocked)
      onSynthUnlocked();
  }));

  Log::info("SidechainAudioProcessor: Plugin initialized");
}

SidechainAudioProcessor::~SidechainAudioProcessor() {
  Log::debug("SidechainAudioProcessor: Destroying");

  // Save drafts and flush all caches to persistent storage before shutdown
  Sidechain::Stores::AppStore::getInstance().saveDrafts();
  Sidechain::Stores::AppStore::getInstance().flushCaches();

  // Shutdown logging last to ensure all log messages are written
  // and to prevent JUCE leak detector warnings for FileOutputStream
  Log::shutdown();
}

//==============================================================================
/** Get the plugin name
 * @return The plugin name as defined by JucePlugin_Name
 */
const juce::String SidechainAudioProcessor::getName() const {
  return JucePlugin_Name;
}

/** Check if the plugin accepts MIDI input
 * @return true if MIDI input is enabled, false otherwise
 */
bool SidechainAudioProcessor::acceptsMidi() const {
#if JucePlugin_WantsMidiInput
  return true;
#else
  return false;
#endif
}

/** Check if the plugin produces MIDI output
 * @return true if MIDI output is enabled, false otherwise
 */
bool SidechainAudioProcessor::producesMidi() const {
#if JucePlugin_ProducesMidiOutput
  return true;
#else
  return false;
#endif
}

/** Check if this is a MIDI effect plugin
 * @return true if configured as MIDI effect, false otherwise
 */
bool SidechainAudioProcessor::isMidiEffect() const {
#if JucePlugin_IsMidiEffect
  return true;
#else
  return false;
#endif
}

/** Get the tail length in seconds
 * @return 0.0 (no tail length for this plugin)
 */
double SidechainAudioProcessor::getTailLengthSeconds() const {
  return 0.0;
}

/** Get the number of programs (presets)
 * @return 1 (minimum required by some hosts)
 */
int SidechainAudioProcessor::getNumPrograms() {
  return 1; // NB: some hosts don't cope very well if you tell them there are 0
            // programs, so this should be at least 1, even if you're not really
            // implementing programs.
}

/** Get the current program index
 * @return 0 (programs not implemented)
 */
int SidechainAudioProcessor::getCurrentProgram() {
  return 0;
}

/** Set the current program
 * @param index Program index (single program only, but tracked for compatibility)
 */
void SidechainAudioProcessor::setCurrentProgram(int index) {
  if (index >= 0 && index < getNumPrograms()) {
    currentProgram = index;
    Log::debug("SidechainAudioProcessor: Program changed to " + juce::String(index));
  }
}

/** Get the name of a program
 * @param index Program index (single program only)
 * @return Program name ("Sidechain" for program 0)
 */
const juce::String SidechainAudioProcessor::getProgramName(int index) {
  if (index == 0) {
    return "Sidechain";
  }
  return {};
}

/** Change the name of a program
 * @param index Program index (ignored - single program is not renameable)
 * @param newName New program name (ignored)
 */
void SidechainAudioProcessor::changeProgramName(int index, const juce::String &newName) {
  // Single-program plugin: program names are fixed
  // Log attempt for debugging purposes
  Log::debug("SidechainAudioProcessor: Attempted to rename program " + juce::String(index) +
             " to '" + newName + "' (not supported)");
}

//==============================================================================
/** Prepare the plugin for audio processing
 * Called by the host before playback starts. Initializes all audio subsystems.
 * @param sampleRate The sample rate of the audio system
 * @param samplesPerBlock The maximum block size for audio processing
 */
void SidechainAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
  currentSampleRate = sampleRate;
  currentBlockSize = samplesPerBlock;

  // Prepare audio capture with current settings
  int numChannels = getTotalNumInputChannels();
  audioCapture.prepare(sampleRate, samplesPerBlock, numChannels);

  // Prepare MIDI capture
  midiCapture.prepare(sampleRate, samplesPerBlock);

  // Prepare audio player for feed playback
  audioPlayer.prepareToPlay(sampleRate, samplesPerBlock);

  // Prepare synth engine (R.2.1)
  synthEngine.prepare(sampleRate, samplesPerBlock);

  Log::info("SidechainAudioProcessor: Prepared - " + juce::String(sampleRate) + "Hz, " + juce::String(samplesPerBlock) +
            " samples, " + juce::String(numChannels) + " channels");
}

/** Release audio resources
 * Called by the host when playback stops. Cleans up audio subsystems.
 */
void SidechainAudioProcessor::releaseResources() {
  Log::debug("SidechainAudioProcessor: Releasing resources");
  audioPlayer.releaseResources();
}

#ifndef JucePlugin_PreferredChannelConfigurations
/** Check if a bus layout is supported
 * Validates that the plugin can handle the requested input/output channel
 * configuration.
 * @param layouts The bus layout to check
 * @return true if the layout is supported, false otherwise
 */
bool SidechainAudioProcessor::isBusesLayoutSupported(const BusesLayout &layouts) const {
#if JucePlugin_IsMidiEffect
  juce::ignoreUnused(layouts);
  return true;
#else
  // This is the place where you check if the layout is supported.
  // In this template code we only support mono or stereo.
  if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono() &&
      layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
    return false;

  // This checks if the input layout matches the output layout
#if !JucePlugin_IsSynth
  if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
    return false;
#endif

  return true;
#endif
}
#endif

/** Process audio and MIDI blocks
 * Main audio processing function called by the host on the audio thread.
 * Captures audio/MIDI for recording and mixes in feed playback audio.
 * @param buffer The audio buffer to process
 * @param midiMessages The MIDI messages for this block
 */
void SidechainAudioProcessor::processBlock(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midiMessages) {
  juce::ScopedNoDenormals noDenormals;

  // Aggregated performance monitoring (Task 4.15)
  // Record per-block timing but only report every 1000 calls to avoid audio
  // thread blocking
  static int processBlockCallCount = 0;
  static double processBlockTotalMs = 0.0;
  auto processBlockStartTime = juce::Time::getMillisecondCounterHiRes();

  auto totalNumInputChannels = getTotalNumInputChannels();
  auto totalNumOutputChannels = getTotalNumOutputChannels();

  // Clear any output channels that don't contain input data
  for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
    buffer.clear(i, 0, buffer.getNumSamples());

  // Read transport info from DAW via AudioPlayHead
  if (auto *playHead = getPlayHead()) {
    if (auto position = playHead->getPosition()) {
      // Get BPM
      if (auto bpm = position->getBpm()) {
        currentBPM.store(*bpm);
        bpmAvailable.store(true);
      } else {
        bpmAvailable.store(false);
      }

      // Detect DAW transport state changes for audio focus
      bool isDAWPlaying = position->getIsPlaying();
      bool wasDAWPlaying = dawTransportPlaying.load();

      if (isDAWPlaying != wasDAWPlaying) {
        dawTransportPlaying.store(isDAWPlaying);
        Log::debug("SidechainAudioProcessor: DAW transport state changed - playing: " +
                   juce::String(isDAWPlaying ? "true" : "false"));

        // Notify audio player on message thread (not audio thread)
        juce::MessageManager::callAsync([this, isDAWPlaying]() {
          if (isDAWPlaying)
            audioPlayer.onDAWTransportStarted();
          else
            audioPlayer.onDAWTransportStopped();
        });
      }
    }
  }

  // Capture audio for recording (lock-free, called on audio thread)
  // This captures the incoming audio before any processing
  audioCapture.captureAudio(buffer);

  // Capture MIDI events for stories (lock-free, called on audio thread)
  midiCapture.captureMIDI(midiMessages, buffer.getNumSamples(), currentSampleRate);

  // Process chord detection for hidden synth easter egg (R.2.1)
  // This runs on every audio block to detect unlock sequences
  chordDetector.processMIDI(midiMessages, currentSampleRate);

  // Process hidden synth if enabled (R.2.1)
  if (synthEnabled.load()) {
    synthEngine.process(buffer, midiMessages);
  }

  // Mix in feed audio playback (adds to the output buffer)
  // This allows users to hear posts while working in their DAW
  audioPlayer.processBlock(buffer, buffer.getNumSamples());

  // Mix in buffer audio player (for story preview)
  if (bufferAudioPlayer != nullptr) {
    bufferAudioPlayer->processBlock(buffer, buffer.getNumSamples());
  }

  // Record aggregated timing (Task 4.15)
  // Only report every 1000 calls to minimize audio thread overhead
  auto processBlockElapsedMs = juce::Time::getMillisecondCounterHiRes() - processBlockStartTime;
  processBlockTotalMs += processBlockElapsedMs;
  processBlockCallCount++;

  if (processBlockCallCount >= 1000) {
    auto avgMs = processBlockTotalMs / processBlockCallCount;
    using namespace Sidechain::Util::Profiling;
    PerformanceMonitor::getInstance()->record("audio::process_block", avgMs,
                                              10.0); // Warn if > 10ms
    processBlockCallCount = 0;
    processBlockTotalMs = 0.0;
  }
}

//==============================================================================
/** Check if the plugin has an editor
 * @return true (this plugin always has a UI for the social feed)
 */
bool SidechainAudioProcessor::hasEditor() const {
  return true; // We want a UI for the social feed
}

/** Create the plugin editor
 * @return A new instance of SidechainAudioProcessorEditor
 */
juce::AudioProcessorEditor *SidechainAudioProcessor::createEditor() {
  Log::info("SidechainAudioProcessor: Creating editor");
  return new SidechainAudioProcessorEditor(*this);
}

//==============================================================================
/** Save plugin state to memory
 * Serializes the plugin's state (authentication, settings) for persistence.
 * @param destData Memory block to write state data to
 */
void SidechainAudioProcessor::getStateInformation(juce::MemoryBlock &destData) {
  // Save plugin state (simplified)
  Log::debug("SidechainAudioProcessor: Saving state");
  auto state = juce::ValueTree("SidechainState");
  state.setProperty("authenticated", authenticated, nullptr);

  if (auto xml = state.createXml())
    copyXmlToBinary(*xml, destData);
}

/** Restore plugin state from memory
 * Deserializes the plugin's state from saved data.
 * @param data Pointer to the state data
 * @param sizeInBytes Size of the state data in bytes
 */
void SidechainAudioProcessor::setStateInformation(const void *data, int sizeInBytes) {
  // Restore plugin state (simplified)
  Log::debug("SidechainAudioProcessor: Restoring state");
  if (auto xml = getXmlFromBinary(data, sizeInBytes)) {
    auto state = juce::ValueTree::fromXml(*xml);
    if (state.isValid()) {
      authenticated = state.getProperty("authenticated", false);
      Log::debug("SidechainAudioProcessor: State restored - authenticated: " +
                 juce::String(authenticated ? "true" : "false"));
    } else {
      Log::warn("SidechainAudioProcessor: Invalid state data");
    }
  } else {
    Log::warn("SidechainAudioProcessor: Failed to parse state data");
  }
}

//==============================================================================
// Audio Recording API
//==============================================================================
/** Start recording audio and MIDI from the DAW
 * Begins capturing both audio and MIDI data simultaneously.
 * Must be called from the message thread.
 */
void SidechainAudioProcessor::startRecording() {
  // Generate a unique recording ID
  juce::String recordingId = juce::Uuid().toString();
  audioCapture.startRecording(recordingId);

  // Start MIDI capture simultaneously
  midiCapture.startCapture();

  Log::info("SidechainAudioProcessor: Started recording - ID: " + recordingId);
}

/** Stop recording and finalize captured data
 * Stops both audio and MIDI capture and prepares the data for export.
 * Must be called from the message thread.
 */
void SidechainAudioProcessor::stopRecording() {
  lastRecordedAudio = audioCapture.stopRecording();

  // Stop MIDI capture
  auto midiEvents = midiCapture.stopCapture();

  double duration = static_cast<double>(lastRecordedAudio.getNumSamples()) / currentSampleRate;
  Log::info("SidechainAudioProcessor: Stopped recording - " + juce::String(lastRecordedAudio.getNumSamples()) +
            " samples, " + juce::String(duration, 2) + " seconds, " + juce::String(midiEvents.size()) + " MIDI events");
}

/** Get the recorded audio buffer
 * Returns the audio captured during the last recording session.
 * @return The recorded audio buffer (empty if no recording was made)
 */
juce::AudioBuffer<float> SidechainAudioProcessor::getRecordedAudio() {
  return lastRecordedAudio;
}

//==============================================================================
/** Get the name of the DAW hosting this plugin
 * Uses NetworkClient's detection method
 * @return DAW name (e.g., "Ableton Live", "FL Studio", "Logic Pro")
 */
juce::String SidechainAudioProcessor::getHostDAWName() const {
  // Use NetworkClient's detection method which handles platform-specific
  // detection
  return NetworkClient::detectDAWName();
}

//==============================================================================
/** Factory function to create new plugin instances
 * Called by the JUCE framework to instantiate the plugin.
 * @return A new instance of SidechainAudioProcessor
 */
juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter() {
  return new SidechainAudioProcessor();
}