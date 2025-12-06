
.. _program_listing_file_plugin_source_models_Story.h:

Program Listing for File Story.h
================================

|exhale_lsh| :ref:`Return to documentation for file <file_plugin_source_models_Story.h>` (``plugin/source/models/Story.h``)

.. |exhale_lsh| unicode:: U+021B0 .. UPWARDS ARROW WITH TIP LEFTWARDS

.. code-block:: cpp

   #pragma once
   
   #include <JuceHeader.h>
   
   //==============================================================================
   struct Story
   {
       juce::String id;
       juce::String userId;
       juce::String audioUrl;
       float audioDuration = 0.0f;      // Duration in seconds (5-60)
       juce::var midiData;              // MIDI events for visualization
       juce::String waveformData;       // SVG waveform
       int bpm = 0;
       juce::String key;
       juce::StringArray genres;
       int viewCount = 0;
       bool viewed = false;             // Has current user viewed this story
       juce::Time createdAt;
       juce::Time expiresAt;
   
       // Associated user info (from API response)
       juce::String username;
       juce::String userDisplayName;
       juce::String userAvatarUrl;
   
       //==============================================================================
       // Helper methods
   
       // Check if story is expired
       bool isExpired() const;
   
       // Get remaining time until expiration
       juce::String getExpirationText() const;
   
       // Check if story has MIDI data
       bool hasMIDI() const;
   
       // Parse from JSON response
       static Story fromJSON(const juce::var& json);
   
       // Convert to JSON for upload
       juce::var toJSON() const;
   };
