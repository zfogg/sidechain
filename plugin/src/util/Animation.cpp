#include "Animation.h"

//==============================================================================
Animation::Animation(int duration, Easing easing)
    : durationMs(duration)
    , easingType(easing)
{
}

Animation::~Animation()
{
    stopTimer();
}

//==============================================================================
void Animation::start()
{
    reversed = false;
    rawProgress = 0.0f;
    easedProgress = 0.0f;
    currentRepeat = 0;
    pingPongDirection = true;
    startTimeMs = juce::Time::currentTimeMillis();
    startTimer(frameIntervalMs);
}

void Animation::startReverse()
{
    reversed = true;
    rawProgress = 1.0f;
    easedProgress = 1.0f;
    currentRepeat = 0;
    pingPongDirection = true;
    startTimeMs = juce::Time::currentTimeMillis();
    startTimer(frameIntervalMs);
}

void Animation::stop()
{
    stopTimer();
}

void Animation::reset()
{
    stopTimer();
    rawProgress = 0.0f;
    easedProgress = 0.0f;
    currentRepeat = 0;
    pingPongDirection = true;
}

//==============================================================================
void Animation::setDuration(int duration)
{
    durationMs = duration;
}

void Animation::setEasing(Easing easing)
{
    easingType = easing;
}

void Animation::setFrameRate(int fps)
{
    frameIntervalMs = fps > 0 ? 1000 / fps : 16;
}

//==============================================================================
float Animation::interpolate(float startValue, float endValue) const
{
    return startValue + (endValue - startValue) * easedProgress;
}

juce::Colour Animation::interpolate(juce::Colour startColor, juce::Colour endColor) const
{
    return startColor.interpolatedWith(endColor, easedProgress);
}

//==============================================================================
void Animation::timerCallback()
{
    auto elapsedMs = juce::Time::currentTimeMillis() - startTimeMs;
    float cycleProgress = juce::jlimit(0.0f, 1.0f, static_cast<float>(elapsedMs) / static_cast<float>(durationMs));

    // Handle ping-pong mode
    if (pingPong && !pingPongDirection)
    {
        cycleProgress = 1.0f - cycleProgress;
    }

    if (reversed)
        cycleProgress = 1.0f - cycleProgress;

    rawProgress = cycleProgress;
    easedProgress = calculateEasing(rawProgress);

    if (onUpdate)
        onUpdate(easedProgress);

    // Check if current cycle is complete
    bool cycleComplete = (pingPong && !pingPongDirection) ? (cycleProgress <= 0.0f) : (cycleProgress >= 1.0f);

    if (cycleComplete)
    {
        // Handle repeat logic
        bool shouldRepeat = (repeatCount < 0) || (currentRepeat < repeatCount);

        if (pingPong && shouldRepeat)
        {
            // Ping-pong: reverse direction and continue
            pingPongDirection = !pingPongDirection;
            startTimeMs = juce::Time::currentTimeMillis();
            currentRepeat++;
        }
        else if (shouldRepeat && !pingPong)
        {
            // Regular repeat: restart from beginning
            startTimeMs = juce::Time::currentTimeMillis();
            currentRepeat++;
        }
        else
        {
            // Animation complete
            stopTimer();
            easedProgress = reversed ? 0.0f : 1.0f;

            if (onComplete)
                onComplete();
        }
    }
}

float Animation::calculateEasing(float t) const
{
    // Custom easing implementations using standard mathematical formulas
    switch (easingType)
    {
        case Easing::Linear:
            return t;

        case Easing::EaseIn:
            // Quadratic ease-in: t^2
            return t * t;

        case Easing::EaseOut:
            // Quadratic ease-out: 1 - (1-t)^2
            return 1.0f - (1.0f - t) * (1.0f - t);

        case Easing::EaseInOut:
            // Quadratic ease-in-out
            return t < 0.5f ? 2.0f * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 2.0f) / 2.0f;

        case Easing::EaseOutCubic:
            // Cubic ease-out: 1 - (1-t)^3
            return 1.0f - std::pow(1.0f - t, 3.0f);

        case Easing::EaseInCubic:
            // Cubic ease-in: t^3
            return t * t * t;

        case Easing::EaseOutBounce:
        {
            // Standard bounce easing algorithm
            const float n1 = 7.5625f;
            const float d1 = 2.75f;

            if (t < 1.0f / d1)
                return n1 * t * t;
            else if (t < 2.0f / d1)
            {
                float t2 = t - 1.5f / d1;
                return n1 * t2 * t2 + 0.75f;
            }
            else if (t < 2.5f / d1)
            {
                float t2 = t - 2.25f / d1;
                return n1 * t2 * t2 + 0.9375f;
            }
            else
            {
                float t2 = t - 2.625f / d1;
                return n1 * t2 * t2 + 0.984375f;
            }
        }

        case Easing::EaseOutBack:
        {
            // Back easing with overshoot
            const float c1 = 1.70158f;
            const float c3 = c1 + 1.0f;
            float t1 = t - 1.0f;
            return 1.0f + c3 * t1 * t1 * t1 + c1 * t1 * t1;
        }

        case Easing::EaseOutElastic:
        {
            // Elastic easing
            if (t == 0.0f) return 0.0f;
            if (t == 1.0f) return 1.0f;
            const float c4 = (2.0f * juce::MathConstants<float>::pi) / 3.0f;
            return std::pow(2.0f, -10.0f * t) * std::sin((t * 10.0f - 0.75f) * c4) + 1.0f;
        }
    }

    return t;  // Default to linear
}
