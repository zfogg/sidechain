
.. _program_listing_file_plugin_source_util_ImageCache.h:

Program Listing for File ImageCache.h
=====================================

|exhale_lsh| :ref:`Return to documentation for file <file_plugin_source_util_ImageCache.h>` (``plugin/source/util/ImageCache.h``)

.. |exhale_lsh| unicode:: U+021B0 .. UPWARDS ARROW WITH TIP LEFTWARDS

.. code-block:: cpp

   #pragma once
   
   #include <JuceHeader.h>
   #include <map>
   #include <list>
   #include <mutex>
   #include <functional>
   
   class NetworkClient;
   
   //==============================================================================
   namespace ImageLoader
   {
       //==========================================================================
       // Types
   
       using ImageCallback = std::function<void(const juce::Image&)>;
   
       //==========================================================================
       // Core API
   
       void load(const juce::String& url,
                 ImageCallback callback,
                 int width = 0,
                 int height = 0);
   
       juce::Image loadSync(const juce::String& url);
   
       bool isCached(const juce::String& url);
   
       juce::Image getCached(const juce::String& url);
   
       void preload(const juce::StringArray& urls);
   
       //==========================================================================
       // Cache Management
   
       void setMaxSize(size_t maxImages);
   
       size_t getSize();
   
       void clear();
   
       void evict(const juce::String& url);
   
       void setNetworkClient(NetworkClient* client);
   
       //==========================================================================
       // Statistics (for debugging)
   
       struct Stats
       {
           size_t cacheHits = 0;
           size_t cacheMisses = 0;
           size_t downloadSuccesses = 0;
           size_t downloadFailures = 0;
           size_t evictions = 0;
       };
   
       Stats getStats();
       void resetStats();
   
       //==========================================================================
       // Drawing Helpers
   
       juce::String getInitials(const juce::String& name);
   
       void drawCircularAvatar(juce::Graphics& g,
                               juce::Rectangle<int> bounds,
                               const juce::Image& image,
                               const juce::String& initials,
                               juce::Colour bgColor,
                               juce::Colour textColor,
                               float fontSize = 14.0f);
   
   }  // namespace ImageLoader
