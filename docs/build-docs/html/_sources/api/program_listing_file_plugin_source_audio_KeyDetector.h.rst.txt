
.. _program_listing_file_plugin_source_audio_KeyDetector.h:

Program Listing for File KeyDetector.h
======================================

|exhale_lsh| :ref:`Return to documentation for file <file_plugin_source_audio_KeyDetector.h>` (``plugin/source/audio/KeyDetector.h``)

.. |exhale_lsh| unicode:: U+021B0 .. UPWARDS ARROW WITH TIP LEFTWARDS

.. code-block:: cpp

   #pragma once
   
   #include <JuceHeader.h>
   
   //==============================================================================
   class KeyDetector
   {
   public:
       //==========================================================================
       // Key representation
       struct Key
       {
           juce::String name;         // Standard name: "A minor", "F# major"
           juce::String shortName;    // Short: "Am", "F#"
           juce::String camelot;      // Camelot: "8A", "4B"
           bool isMajor = true;
           int rootNote = 0;          // 0-11 (C=0, C#=1, ..., B=11)
           float confidence = 0.0f;   // 0.0-1.0 detection confidence
   
           bool isValid() const { return name.isNotEmpty(); }
   
           // Create from standard key string
           static Key fromString(const juce::String& keyStr);
   
           // Get all 24 keys in Camelot order (useful for UI dropdowns)
           static juce::StringArray getAllKeys();
           static juce::StringArray getAllCamelotKeys();
       };
   
       //==========================================================================
       KeyDetector();
       ~KeyDetector();
   
       //==========================================================================
       Key detectKey(const juce::AudioBuffer<float>& buffer,
                     double sampleRate,
                     int numChannels);
   
       Key detectKeyFromFile(const juce::File& audioFile);
   
       //==========================================================================
       static bool isAvailable();
   
       //==========================================================================
       // Key name utilities
   
       static juce::String keyToString(int keyValue);
   
       static juce::String keyToCamelot(int keyValue);
   
       static juce::String getRootNoteName(int keyValue);
   
       static bool isMajorKey(int keyValue);
   
   private:
       class Impl;
       std::unique_ptr<Impl> impl;
   
       JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KeyDetector)
   };
