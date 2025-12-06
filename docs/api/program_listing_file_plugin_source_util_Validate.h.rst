
.. _program_listing_file_plugin_source_util_Validate.h:

Program Listing for File Validate.h
===================================

|exhale_lsh| :ref:`Return to documentation for file <file_plugin_source_util_Validate.h>` (``plugin/source/util/Validate.h``)

.. |exhale_lsh| unicode:: U+021B0 .. UPWARDS ARROW WITH TIP LEFTWARDS

.. code-block:: cpp

   #pragma once
   
   #include <JuceHeader.h>
   
   //==============================================================================
   namespace Validate
   {
       //==========================================================================
       // String Format Validation
   
       bool isEmail(const juce::String& str);
   
       bool isUrl(const juce::String& str);
   
       bool isUsername(const juce::String& str);
   
       bool isDisplayName(const juce::String& str);
   
       bool isUuid(const juce::String& str);
   
       //==========================================================================
       // Range Validation
   
       bool inRange(int val, int min, int max);
   
       bool inRange(float val, float min, float max);
   
       bool lengthInRange(const juce::String& str, int minLen, int maxLen);
   
       //==========================================================================
       // Content Validation
   
       bool isBlank(const juce::String& str);
   
       bool isNotBlank(const juce::String& str);
   
       bool isAlphanumeric(const juce::String& str);
   
       bool isNumeric(const juce::String& str);
   
       bool isValidJson(const juce::String& str);
   
       //==========================================================================
       // Audio/Music Validation
   
       bool isValidBpm(float bpm);
   
       bool isValidKey(const juce::String& key);
   
       bool isValidDuration(float seconds);
   
       //==========================================================================
       // Sanitization
   
       juce::String sanitizeUsername(const juce::String& input);
   
       juce::String sanitizeDisplayName(const juce::String& input);
   
       juce::String escapeHtml(const juce::String& input);
   
       juce::String stripHtml(const juce::String& input);
   
       juce::String normalizeWhitespace(const juce::String& input);
   
       juce::String truncate(const juce::String& input, int maxLength, bool addEllipsis = true);
   
   }  // namespace Validate
