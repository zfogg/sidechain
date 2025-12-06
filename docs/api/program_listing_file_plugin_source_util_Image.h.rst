
.. _program_listing_file_plugin_source_util_Image.h:

Program Listing for File Image.h
================================

|exhale_lsh| :ref:`Return to documentation for file <file_plugin_source_util_Image.h>` (``plugin/source/util/Image.h``)

.. |exhale_lsh| unicode:: U+021B0 .. UPWARDS ARROW WITH TIP LEFTWARDS

.. code-block:: cpp

   #pragma once
   
   #include <JuceHeader.h>
   
   //==============================================================================
   namespace ImageUtils
   {
       void drawCircular(juce::Graphics& g,
                         juce::Rectangle<int> bounds,
                         const juce::Image& image);
   
       void drawCircularAvatar(juce::Graphics& g,
                               juce::Rectangle<int> bounds,
                               const juce::Image& image,
                               const juce::String& initials,
                               juce::Colour backgroundColor = juce::Colour(0xff3a3a3e),
                               juce::Colour textColor = juce::Colours::white,
                               float fontSize = 0.0f);
   
       juce::String getInitials(const juce::String& displayName, int maxChars = 2);
   
       void drawRounded(juce::Graphics& g,
                        juce::Rectangle<int> bounds,
                        const juce::Image& image,
                        float cornerRadius);
   
   }  // namespace ImageUtils
