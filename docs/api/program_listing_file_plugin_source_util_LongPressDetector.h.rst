
.. _program_listing_file_plugin_source_util_LongPressDetector.h:

Program Listing for File LongPressDetector.h
============================================

|exhale_lsh| :ref:`Return to documentation for file <file_plugin_source_util_LongPressDetector.h>` (``plugin/source/util/LongPressDetector.h``)

.. |exhale_lsh| unicode:: U+021B0 .. UPWARDS ARROW WITH TIP LEFTWARDS

.. code-block:: cpp

   #pragma once
   
   #include <JuceHeader.h>
   #include <functional>
   
   //==============================================================================
   class LongPressDetector : private juce::Timer
   {
   public:
       explicit LongPressDetector(int thresholdMs = 500)
           : threshold(thresholdMs)
       {
       }
   
       ~LongPressDetector() override
       {
           stopTimer();
       }
   
       //==========================================================================
       // Control
   
       void start()
       {
           triggered = false;
           startTimer(threshold);
       }
   
       void start(std::function<void()> callback)
       {
           onLongPress = std::move(callback);
           start();
       }
   
       void cancel()
       {
           stopTimer();
       }
   
       void reset()
       {
           cancel();
           triggered = false;
       }
   
       //==========================================================================
       // State
   
       bool isActive() const { return isTimerRunning(); }
   
       bool wasTriggered() const { return triggered; }
   
       //==========================================================================
       // Configuration
   
       void setThreshold(int thresholdMs)
       {
           threshold = thresholdMs;
       }
   
       int getThreshold() const { return threshold; }
   
       //==========================================================================
       // Callbacks
   
       std::function<void()> onLongPress;
   
   private:
       void timerCallback() override
       {
           stopTimer();
           triggered = true;
   
           if (onLongPress)
               onLongPress();
       }
   
       int threshold = 500;
       bool triggered = false;
   
       JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LongPressDetector)
   };
   
   //==============================================================================
   class LongPressWithProgress : private juce::Timer
   {
   public:
       explicit LongPressWithProgress(int thresholdMs = 500, int updateIntervalMs = 16)
           : threshold(thresholdMs)
           , updateInterval(updateIntervalMs)
       {
       }
   
       ~LongPressWithProgress() override
       {
           stopTimer();
       }
   
       //==========================================================================
       // Control
   
       void start()
       {
           triggered = false;
           startTimeMs = juce::Time::currentTimeMillis();
           startTimer(updateInterval);
       }
   
       void cancel()
       {
           stopTimer();
           if (onProgress)
               onProgress(0.0f);
       }
   
       void reset()
       {
           cancel();
           triggered = false;
       }
   
       //==========================================================================
       // State
   
       bool isActive() const { return isTimerRunning(); }
       bool wasTriggered() const { return triggered; }
       float getProgress() const { return currentProgress; }
   
       //==========================================================================
       // Configuration
   
       void setThreshold(int thresholdMs) { threshold = thresholdMs; }
       int getThreshold() const { return threshold; }
   
       //==========================================================================
       // Callbacks
   
       std::function<void(float progress)> onProgress;
       std::function<void()> onLongPress;
   
   private:
       void timerCallback() override
       {
           auto elapsedMs = juce::Time::currentTimeMillis() - startTimeMs;
           currentProgress = juce::jlimit(0.0f, 1.0f, static_cast<float>(elapsedMs) / static_cast<float>(threshold));
   
           if (onProgress)
               onProgress(currentProgress);
   
           if (currentProgress >= 1.0f)
           {
               stopTimer();
               triggered = true;
   
               if (onLongPress)
                   onLongPress();
           }
       }
   
       int threshold = 500;
       int updateInterval = 16;
       juce::int64 startTimeMs = 0;
       float currentProgress = 0.0f;
       bool triggered = false;
   
       JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LongPressWithProgress)
   };
