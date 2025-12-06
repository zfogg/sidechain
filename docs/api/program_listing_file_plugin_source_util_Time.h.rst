
.. _program_listing_file_plugin_source_util_Time.h:

Program Listing for File Time.h
===============================

|exhale_lsh| :ref:`Return to documentation for file <file_plugin_source_util_Time.h>` (``plugin/source/util/Time.h``)

.. |exhale_lsh| unicode:: U+021B0 .. UPWARDS ARROW WITH TIP LEFTWARDS

.. code-block:: cpp

   #pragma once
   
   #include <JuceHeader.h>
   
   //==============================================================================
   namespace TimeUtils
   {
       juce::String formatTimeAgo(const juce::Time& time);
   
       juce::String formatTimeAgoShort(const juce::Time& time);
   }
