
.. _program_listing_file_plugin_source_util_Async.h:

Program Listing for File Async.h
================================

|exhale_lsh| :ref:`Return to documentation for file <file_plugin_source_util_Async.h>` (``plugin/source/util/Async.h``)

.. |exhale_lsh| unicode:: U+021B0 .. UPWARDS ARROW WITH TIP LEFTWARDS

.. code-block:: cpp

   #pragma once
   
   #include <JuceHeader.h>
   #include <functional>
   #include <thread>
   #include <map>
   #include <mutex>
   #include <memory>
   
   //==============================================================================
   namespace Async
   {
       //==========================================================================
       // Background Work with Result
   
       template<typename T>
       void run(std::function<T()> work, std::function<void(T)> onComplete)
       {
           std::thread([work = std::move(work), onComplete = std::move(onComplete)]() {
               T result = work();
               juce::MessageManager::callAsync([result = std::move(result), onComplete]() {
                   if (onComplete)
                       onComplete(result);
               });
           }).detach();
       }
   
       void runVoid(std::function<void()> work, std::function<void()> onComplete = nullptr);
   
       //==========================================================================
       // Delayed Execution
   
       int delay(int delayMs, std::function<void()> callback);
   
       void cancelDelay(int timerId);
   
       //==========================================================================
       // Debouncing
   
       void debounce(const juce::String& key, int delayMs, std::function<void()> callback);
   
       void cancelDebounce(const juce::String& key);
   
       void cancelAllDebounces();
   
       //==========================================================================
       // Throttling
   
       void throttle(const juce::String& key, int periodMs, std::function<void()> callback);
   
       void cancelThrottle(const juce::String& key);
   
   }  // namespace Async
