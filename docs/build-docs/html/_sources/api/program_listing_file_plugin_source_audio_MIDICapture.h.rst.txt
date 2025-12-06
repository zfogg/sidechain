
.. _program_listing_file_plugin_source_audio_MIDICapture.h:

Program Listing for File MIDICapture.h
======================================

|exhale_lsh| :ref:`Return to documentation for file <file_plugin_source_audio_MIDICapture.h>` (``plugin/source/audio/MIDICapture.h``)

.. |exhale_lsh| unicode:: U+021B0 .. UPWARDS ARROW WITH TIP LEFTWARDS

.. code-block:: cpp

   #pragma once
   
   #include <JuceHeader.h>
   #include <atomic>
   #include <vector>
   
   //==============================================================================
   class MIDICapture
   {
   public:
       //==============================================================================
       // MIDI Event structure (matches backend MIDIEvent)
   
       struct MIDIEvent
       {
           double time;       
           juce::String type; 
           int note;         
           int velocity;      
           int channel;      
       };
   
       //==============================================================================
       MIDICapture();
       ~MIDICapture();
   
       //==============================================================================
       // Configuration - call from prepareToPlay() or message thread
       void prepare(double sampleRate, int samplesPerBlock);
       void reset();
   
       //==============================================================================
       // Recording control - call from MESSAGE THREAD only
       void startCapture();
       std::vector<MIDIEvent> stopCapture();
       bool isCapturing() const { return capturing.load(); }
   
       //==============================================================================
       // MIDI capture - call from AUDIO THREAD (processBlock) only
       // MUST be lock-free and allocation-free
       void captureMIDI(const juce::MidiBuffer& midiMessages, int numSamples, double sampleRate);
   
       //==============================================================================
       // MIDI data export - thread-safe
       juce::var getMIDIDataAsJSON() const;
       double getTotalTime() const { return totalTimeSeconds.load(); }
   
       //==============================================================================
       // MIDI data processing (7.5.2.2)
       // Call after stopCapture() to clean up the data before upload
   
       // Normalize MIDI timing (7.5.2.2.1)
       // - Converts timestamps to relative time (0.0 = start of recording)
       // - Rounds to millisecond precision
       // - Handles tempo changes if present
       static std::vector<MIDIEvent> normalizeTiming(const std::vector<MIDIEvent>& events);
   
       // Validate MIDI data (7.5.2.2.2)
       // - Ensures note_on has matching note_off
       // - Removes duplicate events
       // - Filters out invalid notes (outside 0-127)
       // Returns validated events
       static std::vector<MIDIEvent> validateEvents(const std::vector<MIDIEvent>& events);
   
       // Get normalized and validated MIDI data as JSON
       // Convenience method that applies both normalization and validation
       juce::var getNormalizedMIDIDataAsJSON() const;
   
       // Set tempo from DAW (for proper timing normalization)
       void setTempo(double bpm) { currentTempo.store(bpm); }
       double getTempo() const { return currentTempo.load(); }
   
       // Set time signature from DAW
       void setTimeSignature(int numerator, int denominator);
       std::pair<int, int> getTimeSignature() const;
   
   private:
       //==============================================================================
       // Thread-safe state
       std::atomic<bool> capturing { false };
       std::atomic<double> totalTimeSeconds { 0.0 };
       std::atomic<int> currentSamplePosition { 0 };
   
       // MIDI events (protected by mutex for message thread access)
       mutable juce::CriticalSection eventsLock;
       std::vector<MIDIEvent> midiEvents;
   
       // Audio settings
       double currentSampleRate = 44100.0;
       int currentBlockSize = 512;
   
       // Tempo and time signature (from DAW)
       std::atomic<double> currentTempo { 120.0 };
       std::atomic<int> timeSignatureNumerator { 4 };
       std::atomic<int> timeSignatureDenominator { 4 };
   
       // Helper methods
       void addEvent(const MIDIEvent& event);
       double samplePositionToTime(int samplePosition) const;
   };
   
   
