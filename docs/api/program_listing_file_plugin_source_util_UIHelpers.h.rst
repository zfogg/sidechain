
.. _program_listing_file_plugin_source_util_UIHelpers.h:

Program Listing for File UIHelpers.h
====================================

|exhale_lsh| :ref:`Return to documentation for file <file_plugin_source_util_UIHelpers.h>` (``plugin/source/util/UIHelpers.h``)

.. |exhale_lsh| unicode:: U+021B0 .. UPWARDS ARROW WITH TIP LEFTWARDS

.. code-block:: cpp

   #pragma once
   
   #include <JuceHeader.h>
   
   //==============================================================================
   namespace UI
   {
       //==========================================================================
       // Card/Panel Drawing
   
       void drawCard(juce::Graphics& g,
                     juce::Rectangle<float> bounds,
                     juce::Colour fillColor,
                     juce::Colour borderColor = juce::Colours::transparentBlack,
                     float cornerRadius = 8.0f,
                     float borderWidth = 1.0f);
   
       void drawCard(juce::Graphics& g,
                     juce::Rectangle<int> bounds,
                     juce::Colour fillColor,
                     juce::Colour borderColor = juce::Colours::transparentBlack,
                     float cornerRadius = 8.0f,
                     float borderWidth = 1.0f);
   
       void drawCardWithHover(juce::Graphics& g,
                              juce::Rectangle<int> bounds,
                              juce::Colour normalColor,
                              juce::Colour hoverColor,
                              juce::Colour borderColor,
                              bool isHovered,
                              float cornerRadius = 8.0f);
   
       //==========================================================================
       // Badge/Tag Drawing
   
       void drawBadge(juce::Graphics& g,
                      juce::Rectangle<int> bounds,
                      const juce::String& text,
                      juce::Colour bgColor,
                      juce::Colour textColor,
                      float fontSize = 11.0f,
                      float cornerRadius = 4.0f);
   
       juce::Rectangle<int> drawPillBadge(juce::Graphics& g,
                                           int x, int y,
                                           const juce::String& text,
                                           juce::Colour bgColor,
                                           juce::Colour textColor,
                                           float fontSize = 11.0f,
                                           int hPadding = 8,
                                           int vPadding = 4);
   
       //==========================================================================
       // Button Drawing
   
       void drawButton(juce::Graphics& g,
                       juce::Rectangle<int> bounds,
                       const juce::String& text,
                       juce::Colour bgColor,
                       juce::Colour textColor,
                       bool isHovered = false,
                       float cornerRadius = 6.0f);
   
       void drawOutlineButton(juce::Graphics& g,
                              juce::Rectangle<int> bounds,
                              const juce::String& text,
                              juce::Colour borderColor,
                              juce::Colour textColor,
                              bool isHovered = false,
                              float cornerRadius = 6.0f);
   
       //==========================================================================
       // Icon Drawing
   
       void drawIconButton(juce::Graphics& g,
                           juce::Rectangle<int> bounds,
                           juce::Colour bgColor,
                           bool isHovered = false);
   
       void drawIcon(juce::Graphics& g,
                     juce::Rectangle<int> bounds,
                     const juce::String& icon,
                     juce::Colour color,
                     float fontSize = 16.0f);
   
       //==========================================================================
       // Progress/Status Drawing
   
       void drawProgressBar(juce::Graphics& g,
                            juce::Rectangle<int> bounds,
                            float progress,  // 0.0 to 1.0
                            juce::Colour bgColor,
                            juce::Colour fillColor,
                            float cornerRadius = 4.0f);
   
       void drawLoadingSpinner(juce::Graphics& g,
                               juce::Rectangle<int> bounds,
                               juce::Colour color,
                               float rotation);  // In radians
   
       //==========================================================================
       // Separator Drawing
   
       void drawDivider(juce::Graphics& g,
                        int x, int y, int width,
                        juce::Colour color,
                        float thickness = 1.0f);
   
       void drawVerticalDivider(juce::Graphics& g,
                                int x, int y, int height,
                                juce::Colour color,
                                float thickness = 1.0f);
   
       //==========================================================================
       // Text Utilities
   
       juce::String truncateWithEllipsis(const juce::String& text,
                                          const juce::Font& font,
                                          int maxWidth);
   
       void drawTruncatedText(juce::Graphics& g,
                              const juce::String& text,
                              juce::Rectangle<int> bounds,
                              juce::Colour color,
                              juce::Justification justification = juce::Justification::centredLeft);
   
       int getTextWidth(const juce::Graphics& g, const juce::String& text);
       int getTextWidth(const juce::Font& font, const juce::String& text);
   
       //==========================================================================
       // Shadow/Effects
   
       void drawDropShadow(juce::Graphics& g,
                           juce::Rectangle<int> bounds,
                           juce::Colour shadowColor,
                           int radius = 4,
                           juce::Point<int> offset = {0, 2});
   
       //==========================================================================
       // Tooltip
   
       void drawTooltip(juce::Graphics& g,
                        juce::Rectangle<int> bounds,
                        const juce::String& text,
                        juce::Colour bgColor,
                        juce::Colour textColor);
   
   }  // namespace UI
