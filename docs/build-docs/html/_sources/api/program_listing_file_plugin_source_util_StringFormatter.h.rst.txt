
.. _program_listing_file_plugin_source_util_StringFormatter.h:

Program Listing for File StringFormatter.h
==========================================

|exhale_lsh| :ref:`Return to documentation for file <file_plugin_source_util_StringFormatter.h>` (``plugin/source/util/StringFormatter.h``)

.. |exhale_lsh| unicode:: U+021B0 .. UPWARDS ARROW WITH TIP LEFTWARDS

.. code-block:: cpp

   #pragma once
   
   #include <JuceHeader.h>
   
   //==============================================================================
   namespace StringFormatter
   {
       //==========================================================================
       // Count Formatting (K, M, B suffixes)
   
       juce::String formatCount(int value);
   
       juce::String formatCount(int value, int decimals);
   
       juce::String formatLargeNumber(int64_t value);
   
       //==========================================================================
       // Duration Formatting
   
       juce::String formatDuration(double seconds);
   
       juce::String formatDurationMMSS(double seconds);
   
       juce::String formatDurationHMS(double seconds);
   
       juce::String formatMilliseconds(int ms);
   
       //==========================================================================
       // Percentage Formatting
   
       juce::String formatPercentage(float value);
   
       juce::String formatPercentage(float value, int decimals);
   
       //==========================================================================
       // Music-specific Formatting
   
       juce::String formatBPM(double bpm);
   
       juce::String formatBPMValue(double bpm);
   
       juce::String formatConfidence(float confidence);
   
       juce::String formatKeyLong(const juce::String& key);
   
       //==========================================================================
       // Social/Engagement Formatting
   
       juce::String formatFollowers(int count);
   
       juce::String formatLikes(int count);
   
       juce::String formatComments(int count);
   
       juce::String formatPlays(int count);
   
       //==========================================================================
       // Time Ago Formatting
   
       juce::String formatTimeAgo(const juce::Time& timestamp);
   
       juce::String formatTimeAgo(const juce::String& isoTimestamp);
   
   }  // namespace StringFormatter
