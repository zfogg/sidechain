
.. _program_listing_file_plugin_source_models_AggregatedFeedResponse.h:

Program Listing for File AggregatedFeedResponse.h
=================================================

|exhale_lsh| :ref:`Return to documentation for file <file_plugin_source_models_AggregatedFeedResponse.h>` (``plugin/source/models/AggregatedFeedResponse.h``)

.. |exhale_lsh| unicode:: U+021B0 .. UPWARDS ARROW WITH TIP LEFTWARDS

.. code-block:: cpp

   #pragma once
   
   #include <JuceHeader.h>
   #include "AggregatedFeedGroup.h"
   
   //==============================================================================
   struct AggregatedFeedResponse
   {
       juce::Array<AggregatedFeedGroup> groups;
       int limit = 20;
       int offset = 0;
       int total = 0;
       bool hasMore = false;
       juce::String error;
   };
