
.. _program_listing_file_plugin_source_util_Animation.h:

Program Listing for File Animation.h
====================================

|exhale_lsh| :ref:`Return to documentation for file <file_plugin_source_util_Animation.h>` (``plugin/source/util/Animation.h``)

.. |exhale_lsh| unicode:: U+021B0 .. UPWARDS ARROW WITH TIP LEFTWARDS

.. code-block:: cpp

   #pragma once
   
   #include <JuceHeader.h>
   #include <functional>
   
   //==============================================================================
   class Animation : private juce::Timer
   {
   public:
       //==========================================================================
       // Easing Types
   
       enum class Easing
       {
           Linear,         // No easing, constant speed
           EaseIn,         // Slow start, fast end
           EaseOut,        // Fast start, slow end
           EaseInOut,      // Slow start and end
           EaseOutCubic,   // Smooth deceleration (default for UI)
           EaseInCubic,    // Smooth acceleration
           EaseOutBounce,  // Bouncy end
           EaseOutBack,    // Slight overshoot
           EaseOutElastic  // Elastic spring effect
       };
   
       //==========================================================================
       // Construction
   
       explicit Animation(int durationMs = 300, Easing easing = Easing::EaseOutCubic);
   
       ~Animation() override;
   
       //==========================================================================
       // Control
   
       void start();
   
       void startReverse();
   
       void stop();
   
       void reset();
   
       bool isRunning() const { return isTimerRunning(); }
   
       //==========================================================================
       // Configuration
   
       void setDuration(int durationMs);
   
       void setEasing(Easing easing);
   
       void setFrameRate(int fps);
   
       void setRepeatCount(int count) { repeatCount = count; }
   
       void setPingPong(bool enabled) { pingPong = enabled; }
   
       //==========================================================================
       // Progress
   
       float getRawProgress() const { return rawProgress; }
   
       float getProgress() const { return easedProgress; }
   
       float interpolate(float startValue, float endValue) const;
   
       juce::Colour interpolate(juce::Colour startColor, juce::Colour endColor) const;
   
       //==========================================================================
       // Callbacks
   
       std::function<void(float progress)> onUpdate;
   
       std::function<void()> onComplete;
   
   private:
       void timerCallback() override;
       float calculateEasing(float t) const;
   
       int durationMs = 300;
       int frameIntervalMs = 16;  // ~60 FPS
       Easing easingType = Easing::EaseOutCubic;
       int repeatCount = 0;  // 0 = no repeat, -1 = infinite
       bool pingPong = false;
   
       float rawProgress = 0.0f;
       float easedProgress = 0.0f;
       bool reversed = false;
       int currentRepeat = 0;
       bool pingPongDirection = true;  // true = forward, false = backward
   
       juce::int64 startTimeMs = 0;
   
       JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Animation)
   };
   
   //==============================================================================
   template <typename T>
   class AnimationValue
   {
   public:
       AnimationValue(T initialValue, int durationMs = 300, Animation::Easing easing = Animation::Easing::EaseOutCubic)
           : currentValue(initialValue)
           , targetValue(initialValue)
           , animation(durationMs, easing)
       {
           animation.onUpdate = [this](float progress) {
               currentValue = static_cast<T>(startValue + (targetValue - startValue) * progress);
               if (onValueChanged)
                   onValueChanged(currentValue);
           };
   
           animation.onComplete = [this]() {
               currentValue = targetValue;
               if (onAnimationComplete)
                   onAnimationComplete();
           };
       }
   
       void animateTo(T target)
       {
           if (target == targetValue && !animation.isRunning())
               return;
   
           startValue = currentValue;
           targetValue = target;
           animation.start();
       }
   
       void setImmediate(T value)
       {
           animation.stop();
           currentValue = value;
           targetValue = value;
           startValue = value;
           if (onValueChanged)
               onValueChanged(currentValue);
       }
   
       T getValue() const { return currentValue; }
       T getTarget() const { return targetValue; }
       bool isAnimating() const { return animation.isRunning(); }
   
       std::function<void(T)> onValueChanged;
       std::function<void()> onAnimationComplete;
   
   private:
       T currentValue;
       T targetValue;
       T startValue;
       Animation animation;
   };
