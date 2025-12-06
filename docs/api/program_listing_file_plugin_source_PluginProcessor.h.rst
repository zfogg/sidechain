
.. _program_listing_file_plugin_source_PluginProcessor.h:

Program Listing for File PluginProcessor.h
==========================================

|exhale_lsh| :ref:`Return to documentation for file <file_plugin_source_PluginProcessor.h>` (``plugin/source/PluginProcessor.h``)

.. |exhale_lsh| unicode:: U+021B0 .. UPWARDS ARROW WITH TIP LEFTWARDS

.. code-block:: cpp

   #pragma once
   
   #include <JuceHeader.h>
   #include "audio/AudioCapture.h"
   #include "audio/HttpAudioPlayer.h"
   #include "audio/MIDICapture.h"
   
   class BufferAudioPlayer;
   
   //==============================================================================
   class SidechainAudioProcessor : public juce::AudioProcessor
   {
   public:
       //==============================================================================
       SidechainAudioProcessor();
       ~SidechainAudioProcessor() override;
   
       //==============================================================================
       void prepareToPlay (double sampleRate, int samplesPerBlock) override;
       void releaseResources() override;
   
      #ifndef JucePlugin_PreferredChannelConfigurations
       bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
      #endif
   
       void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
   
       //==============================================================================
       juce::AudioProcessorEditor* createEditor() override;
       bool hasEditor() const override;
   
       //==============================================================================
       const juce::String getName() const override;
   
       bool acceptsMidi() const override;
       bool producesMidi() const override;
       bool isMidiEffect() const override;
       double getTailLengthSeconds() const override;
   
       //==============================================================================
       int getNumPrograms() override;
       int getCurrentProgram() override;
       void setCurrentProgram (int index) override;
       const juce::String getProgramName (int index) override;
       void changeProgramName (int index, const juce::String& newName) override;
   
       //==============================================================================
       void getStateInformation (juce::MemoryBlock& destData) override;
       void setStateInformation (const void* data, int sizeInBytes) override;
   
       //==============================================================================
       // Authentication state
   
       bool isAuthenticated() const { return authenticated; }
   
       void setAuthenticated(bool auth) { authenticated = auth; }
   
       //==============================================================================
       // Audio Recording API (called from Editor/UI thread)
   
       void startRecording();
   
       void stopRecording();
   
       bool isRecording() const { return audioCapture.isRecording(); }
   
       juce::AudioBuffer<float> getRecordedAudio();
   
       juce::var getCapturedMIDIData() const { return midiCapture.getMIDIDataAsJSON(); }
   
       bool hasMIDIData() const { return midiCapture.isCapturing() || midiCapture.getTotalTime() > 0.0; }
   
       //==============================================================================
       // Recording info
   
       double getRecordingLengthSeconds() const { return audioCapture.getRecordingLengthSeconds(); }
   
       double getMaxRecordingLengthSeconds() const { return audioCapture.getMaxRecordingLengthSeconds(); }
   
       float getRecordingProgress() const { return audioCapture.getRecordingProgress(); }
   
       bool isRecordingBufferFull() const { return audioCapture.isBufferFull(); }
   
       //==============================================================================
       // Level metering (for UI display)
   
       float getPeakLevel(int channel) const { return audioCapture.getPeakLevel(channel); }
   
       float getRMSLevel(int channel) const { return audioCapture.getRMSLevel(channel); }
   
       //==============================================================================
       // Sample rate (for UI calculations)
   
       double getCurrentSampleRate() const { return currentSampleRate; }
   
       //==============================================================================
       // DAW Transport Info (BPM detection via AudioPlayHead)
   
       double getCurrentBPM() const { return currentBPM.load(); }
   
       bool isBPMAvailable() const { return bpmAvailable.load(); }
   
       //==============================================================================
       // Audio Playback (for feed audio)
   
       HttpAudioPlayer& getAudioPlayer() { return audioPlayer; }
   
       const HttpAudioPlayer& getAudioPlayer() const { return audioPlayer; }
   
       void setBufferAudioPlayer(BufferAudioPlayer* player) { bufferAudioPlayer = player; }
   
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
   
       JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SidechainAudioProcessor)
   };
