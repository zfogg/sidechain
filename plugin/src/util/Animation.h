#pragma once

#include <JuceHeader.h>
#include <functional>

//==============================================================================
/**
 * Animation - Timer-based animation with easing functions
 *
 * Provides a simple way to animate values over time with various easing
 * functions. The animation runs on the message thread via juce::Timer.
 *
 * Usage:
 *   Animation anim(300, Animation::Easing::EaseOutCubic);
 *   anim.onUpdate = [this](float progress) { alpha = progress; repaint(); };
 *   anim.onComplete = [this]() { isAnimating = false; };
 *   anim.start();
 */
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

    /**
     * Create an animation with duration and easing type.
     * @param durationMs Duration in milliseconds
     * @param easing Easing function to use
     */
    explicit Animation(int durationMs = 300, Easing easing = Easing::EaseOutCubic);

    ~Animation() override;

    //==========================================================================
    // Control

    /**
     * Start the animation from the beginning.
     */
    void start();

    /**
     * Start the animation in reverse (1.0 to 0.0).
     */
    void startReverse();

    /**
     * Stop the animation at current progress.
     */
    void stop();

    /**
     * Stop and reset to initial state.
     */
    void reset();

    /**
     * Check if animation is currently running.
     */
    bool isRunning() const { return isTimerRunning(); }

    //==========================================================================
    // Configuration

    /**
     * Set animation duration.
     */
    void setDuration(int durationMs);

    /**
     * Set easing function.
     */
    void setEasing(Easing easing);

    /**
     * Set the frame rate (default 60 FPS).
     */
    void setFrameRate(int fps);

    /**
     * Set repeat mode - how many times to repeat (0 = no repeat, -1 = infinite).
     * Default is 0 (play once).
     */
    void setRepeatCount(int count) { repeatCount = count; }

    /**
     * Enable ping-pong mode - animation plays forward then backward.
     * When combined with repeat, creates a back-and-forth effect.
     */
    void setPingPong(bool enabled) { pingPong = enabled; }

    //==========================================================================
    // Progress

    /**
     * Get raw progress (0.0 to 1.0, linear).
     */
    float getRawProgress() const { return rawProgress; }

    /**
     * Get eased progress (0.0 to 1.0, with easing applied).
     */
    float getProgress() const { return easedProgress; }

    /**
     * Interpolate between two values based on current progress.
     */
    float interpolate(float startValue, float endValue) const;

    /**
     * Interpolate between two colors based on current progress.
     */
    juce::Colour interpolate(juce::Colour startColor, juce::Colour endColor) const;

    //==========================================================================
    // Callbacks

    /**
     * Called on each animation frame with eased progress (0.0 to 1.0).
     */
    std::function<void(float progress)> onUpdate;

    /**
     * Called when animation completes.
     */
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
/**
 * AnimationValue - Animated value wrapper
 *
 * Convenience class for animating a single value.
 *
 * Usage:
 *   AnimationValue<float> opacity(0.0f, 1.0f, 300);
 *   opacity.onValueChanged = [this](float v) { alpha = v; repaint(); };
 *   opacity.animateTo(1.0f);
 */
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
