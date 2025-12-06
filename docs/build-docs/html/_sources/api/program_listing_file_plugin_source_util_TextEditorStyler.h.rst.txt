
.. _program_listing_file_plugin_source_util_TextEditorStyler.h:

Program Listing for File TextEditorStyler.h
===========================================

|exhale_lsh| :ref:`Return to documentation for file <file_plugin_source_util_TextEditorStyler.h>` (``plugin/source/util/TextEditorStyler.h``)

.. |exhale_lsh| unicode:: U+021B0 .. UPWARDS ARROW WITH TIP LEFTWARDS

.. code-block:: cpp

   #pragma once
   
   #include <JuceHeader.h>
   
   //==============================================================================
   namespace TextEditorStyler
   {
       //==========================================================================
       // Standard Styling
   
       void style(juce::TextEditor& editor, const juce::String& placeholder = "");
   
       void stylePassword(juce::TextEditor& editor, const juce::String& placeholder = "Password");
   
       void styleMultiline(juce::TextEditor& editor, const juce::String& placeholder = "");
   
       //==========================================================================
       // Specialized Styling
   
       void styleEmail(juce::TextEditor& editor, const juce::String& placeholder = "Email");
   
       void styleUsername(juce::TextEditor& editor, const juce::String& placeholder = "Username");
   
       void styleNumeric(juce::TextEditor& editor, const juce::String& placeholder = "");
   
       void styleUrl(juce::TextEditor& editor, const juce::String& placeholder = "https://");
   
       //==========================================================================
       // Configuration
   
       void setInputRestrictions(juce::TextEditor& editor, int maxLength, const juce::String& allowedChars = "");
   
       void setPlaceholder(juce::TextEditor& editor, const juce::String& placeholder);
   
       void styleReadOnly(juce::TextEditor& editor);
   
       void setErrorState(juce::TextEditor& editor, bool hasError);
   
       void clearErrorState(juce::TextEditor& editor);
   
   }  // namespace TextEditorStyler
