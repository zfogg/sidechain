#pragma once

#include <JuceHeader.h>
#include "audio/AudioCapture.h"
#include "audio/HttpAudioPlayer.h"
#include "audio/MIDICapture.h"
#include "audio/ChordSequenceDetector.h"
#include "audio/SynthEngine.h"

class BufferAudioPlayer;

//==============================================================================
/**
 * Sidechain Audio Plugin Processor
 *
 * Main plugin class that handles audio processing and recording.
 * The processor captures audio from the DAW for sharing on the social feed.
 */
class SidechainAudioProcessor : public juce::AudioProcessor
{
public:
    //==============================================================================
    SidechainAudioProcessor();
    ~SidechainAudioProcessor() override;

    //==============================================================================
    /** Prepare the processor for audio playback
     *  @param sampleRate Sample rate in Hz
     *  @param samplesPerBlock Maximum block size
     */
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;

    /** Release audio resources */
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    /** Check if a bus layout is supported
     *  @param layouts Bus layout configuration
     *  @return true if layout is supported
     */
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    /** Process audio and MIDI blocks
     *  @param buffer Audio buffer to process
     *  @param midiMessages MIDI messages to process
     */
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    /** Create the plugin editor
     *  @return Pointer to the editor component
     */
    juce::AudioProcessorEditor* createEditor() override;

    /** Check if plugin has an editor
     *  @return true if editor is available
     */
    bool hasEditor() const override;

    //==============================================================================
    /** Get plugin name
     *  @return Plugin name string
     */
    const juce::String getName() const override;

    /** Check if plugin accepts MIDI input
     *  @return true if MIDI input is accepted
     */
    bool acceptsMidi() const override;

    /** Check if plugin produces MIDI output
     *  @return true if MIDI output is produced
     */
    bool producesMidi() const override;

    /** Check if plugin is a MIDI effect
     *  @return false (this is an audio plugin)
     */
    bool isMidiEffect() const override;

    /** Get plugin tail length in seconds
     *  @return Tail length in seconds
     */
    double getTailLengthSeconds() const override;

    //==============================================================================
    /** Get number of programs
     *  @return Number of programs (0 for this plugin)
     */
    int getNumPrograms() override;

    /** Get current program index
     *  @return Current program index
     */
    int getCurrentProgram() override;

    /** Set current program
     *  @param index Program index
     */
    void setCurrentProgram (int index) override;

    /** Get program name
     *  @param index Program index
     *  @return Program name
     */
    const juce::String getProgramName (int index) override;

    /** Change program name
     *  @param index Program index
     *  @param newName New program name
     */
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    /** Save plugin state
     *  @param destData Memory block to write state to
     */
    void getStateInformation (juce::MemoryBlock& destData) override;

    /** Restore plugin state
     *  @param data State data to restore
     *  @param sizeInBytes Size of state data in bytes
     */
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    // Authentication state

    /** Check if the user is authenticated
     * @return true if authenticated, false otherwise
     */
    bool isAuthenticated() const { return authenticated; }

    /** Set the authentication state
     * @param auth true if authenticated, false otherwise
     */
    void setAuthenticated(bool auth) { authenticated = auth; }

    //==============================================================================
    // Audio Recording API (called from Editor/UI thread)

    /** Start recording audio from the DAW
     * Must be called from the message thread
     */
    void startRecording();

    /** Stop recording and finalize the audio buffer
     * Must be called from the message thread
     */
    void stopRecording();

    /** Check if currently recording
     * @return true if recording is active
     */
    bool isRecording() const { return audioCapture.isRecording(); }

    /** Get the recorded audio buffer
     * Call this after stopRecording() to get the captured audio
     * @return The recorded audio buffer
     */
    juce::AudioBuffer<float> getRecordedAudio();

    /** Get captured MIDI data as JSON
     * @return JSON representation of captured MIDI events
     */
    juce::var getCapturedMIDIData() const { return midiCapture.getMIDIDataAsJSON(); }

    /** Check if MIDI data has been captured
     * @return true if MIDI capture is active or has recorded data
     */
    bool hasMIDIData() const { return midiCapture.isCapturing() || midiCapture.getTotalTime() > 0.0; }

    //==============================================================================
    // Recording info

    /** Get the current recording length in seconds
     * @return Recording duration in seconds
     */
    double getRecordingLengthSeconds() const { return audioCapture.getRecordingLengthSeconds(); }

    /** Get the maximum allowed recording length in seconds
     * @return Maximum recording duration in seconds
     */
    double getMaxRecordingLengthSeconds() const { return audioCapture.getMaxRecordingLengthSeconds(); }

    /** Get recording progress as a normalized value
     * @return Progress from 0.0 to 1.0
     */
    float getRecordingProgress() const { return audioCapture.getRecordingProgress(); }

    /** Check if the recording buffer is full
     * @return true if buffer has reached maximum capacity
     */
    bool isRecordingBufferFull() const { return audioCapture.isBufferFull(); }

    //==============================================================================
    // Level metering (for UI display)

    /** Get peak level for a channel
     * @param channel Channel index (0-based)
     * @return Peak level (0.0 to 1.0+)
     */
    float getPeakLevel(int channel) const { return audioCapture.getPeakLevel(channel); }

    /** Get RMS level for a channel
     * @param channel Channel index (0-based)
     * @return RMS level (0.0 to 1.0)
     */
    float getRMSLevel(int channel) const { return audioCapture.getRMSLevel(channel); }

    //==============================================================================
    // Sample rate (for UI calculations)

    /** Get the current sample rate
     * @return Sample rate in Hz
     */
    double getCurrentSampleRate() const { return currentSampleRate; }

    //==============================================================================
    // DAW Transport Info (BPM detection via AudioPlayHead)

    /** Get the current BPM from the DAW transport
     * Returns 0.0 if BPM is not available from the host
     * @return Current BPM, or 0.0 if unavailable
     */
    double getCurrentBPM() const { return currentBPM.load(); }

    /** Check if BPM information is available from the DAW
     * @return true if BPM data is available
     */
    bool isBPMAvailable() const { return bpmAvailable.load(); }

    /** Get the name of the DAW hosting this plugin
     * @return DAW name (e.g., "Ableton Live", "FL Studio", "Logic Pro")
     */
    juce::String getHostDAWName() const;

    //==============================================================================
    // Audio Playback (for feed audio)

    /** Get the HTTP audio player for feed playback
     * @return Reference to the audio player
     */
    HttpAudioPlayer& getAudioPlayer() { return audioPlayer; }

    /** Get the HTTP audio player for feed playback (const)
     * @return Const reference to the audio player
     */
    const HttpAudioPlayer& getAudioPlayer() const { return audioPlayer; }

    /** Set the buffer audio player for story preview
     * Called by StoryRecordingComponent to enable story audio preview
     * @param player Pointer to the buffer audio player
     */
    void setBufferAudioPlayer(BufferAudioPlayer* player) { bufferAudioPlayer = player; }

    //==============================================================================
    // Hidden Synth Easter Egg (R.2.1)

    /** Get the chord sequence detector for unlock sequences
     * @return Reference to the chord detector
     */
    ChordSequenceDetector& getChordDetector() { return chordDetector; }

    /** Get the hidden synth engine
     * @return Reference to the synth engine
     */
    SynthEngine& getSynthEngine() { return synthEngine; }

    /** Check if the hidden synth has been unlocked
     * @return true if any synth unlock sequence has been triggered
     */
    bool isSynthUnlocked() const { return synthUnlocked.load(); }

    /** Set synth unlocked state (called by chord detector callback)
     * @param unlocked true to unlock synth
     */
    void setSynthUnlocked(bool unlocked) { synthUnlocked.store(unlocked); }

    /** Enable or disable the synth audio output
     * @param enabled true to enable synth audio
     */
    void setSynthEnabled(bool enabled) { synthEnabled.store(enabled); }

    /** Check if synth audio is enabled
     * @return true if synth is producing audio
     */
    bool isSynthEnabled() const { return synthEnabled.load(); }

    /** Callback when synth is unlocked - set by editor */
    std::function<void()> onSynthUnlocked;

private:
    //==============================================================================
    // Audio capture system
    AudioCapture audioCapture;
    juce::AudioBuffer<float> lastRecordedAudio;

    // MIDI capture system (for stories)
    MIDICapture midiCapture;

    // Audio playback for feed
    HttpAudioPlayer audioPlayer;

    // Buffer audio player for story preview (set by StoryRecordingComponent)
    BufferAudioPlayer* bufferAudioPlayer = nullptr;

    // Audio settings (cached from prepareToPlay)
    double currentSampleRate = 44100.0;
    int currentBlockSize = 512;

    // State
    bool authenticated = false;

    // DAW transport info (updated on audio thread, read from UI thread)
    std::atomic<double> currentBPM { 0.0 };
    std::atomic<bool> bpmAvailable { false };
    std::atomic<bool> dawTransportPlaying { false };

    // Hidden synth easter egg (R.2.1)
    ChordSequenceDetector chordDetector;
    SynthEngine synthEngine;
    std::atomic<bool> synthUnlocked { false };
    std::atomic<bool> synthEnabled { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SidechainAudioProcessor)
};