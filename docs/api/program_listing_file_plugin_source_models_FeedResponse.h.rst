
.. _program_listing_file_plugin_source_models_FeedResponse.h:

Program Listing for File FeedResponse.h
=======================================

|exhale_lsh| :ref:`Return to documentation for file <file_plugin_source_models_FeedResponse.h>` (``plugin/source/models/FeedResponse.h``)

.. |exhale_lsh| unicode:: U+021B0 .. UPWARDS ARROW WITH TIP LEFTWARDS

.. code-block:: cpp

   #pragma once
   
   #include <JuceHeader.h>
   #include "FeedPost.h"
   
   //==============================================================================
   struct FeedResponse
   {
       juce::Array<FeedPost> posts;
       int limit = 20;
       int offset = 0;
       int total = 0;
       bool hasMore = false;
       juce::String error;        // Non-empty if there was an error
   };
