#pragma once

#include <cmath>
#include <functional>

namespace Sidechain {
namespace UI {
namespace Animations {

/**
 * Easing - Easing functions for smooth animations
 *
 * Standard easing functions based on Robert Penner's easing equations.
 * All functions take normalized time t in [0.0, 1.0] and return eased value.
 *
 * Example:
 *   auto easeOutCubic = Easing::easeOutCubic(0.5);  // returns ~0.875
 *   auto customEase = Easing::create("easeInOutQuad")(t);
 *
 * Reference: https://easings.net/
 */
class Easing
{
public:
    // Function type for easing functions
    using EasingFunction = std::function<float(float)>;

    // ========== Linear ==========

    /**
     * Linear interpolation (no acceleration)
     * t is directly returned
     */
    static float linear(float t)
    {
        return t;
    }

    // ========== Quadratic (t^2) ==========

    /**
     * Quadratic ease-in: starts slow, accelerates
     * f(t) = t^2
     */
    static float easeInQuad(float t)
    {
        return t * t;
    }

    /**
     * Quadratic ease-out: starts fast, decelerates
     * f(t) = 1 - (1-t)^2
     */
    static float easeOutQuad(float t)
    {
        return 1.0f - (1.0f - t) * (1.0f - t);
    }

    /**
     * Quadratic ease-in-out: slow start, fast middle, slow end
     * f(t) = t < 0.5 ? 2*t^2 : 1 - 2*(1-t)^2
     */
    static float easeInOutQuad(float t)
    {
        return t < 0.5f ? 2.0f * t * t : 1.0f - 2.0f * (1.0f - t) * (1.0f - t);
    }

    // ========== Cubic (t^3) ==========

    /**
     * Cubic ease-in: slower than quadratic
     * f(t) = t^3
     */
    static float easeInCubic(float t)
    {
        return t * t * t;
    }

    /**
     * Cubic ease-out: smoother deceleration
     * f(t) = 1 - (1-t)^3
     */
    static float easeOutCubic(float t)
    {
        float one_minus_t = 1.0f - t;
        return 1.0f - one_minus_t * one_minus_t * one_minus_t;
    }

    /**
     * Cubic ease-in-out: smooth acceleration and deceleration
     * f(t) = t < 0.5 ? 4*t^3 : 1 - 4*(1-t)^3
     */
    static float easeInOutCubic(float t)
    {
        if (t < 0.5f)
        {
            return 4.0f * t * t * t;
        }
        else
        {
            float one_minus_t = 1.0f - t;
            return 1.0f - 4.0f * one_minus_t * one_minus_t * one_minus_t;
        }
    }

    // ========== Quartic (t^4) ==========

    /**
     * Quartic ease-in: even slower than cubic
     * f(t) = t^4
     */
    static float easeInQuart(float t)
    {
        return t * t * t * t;
    }

    /**
     * Quartic ease-out: smooth, gradual deceleration
     * f(t) = 1 - (1-t)^4
     */
    static float easeOutQuart(float t)
    {
        float one_minus_t = 1.0f - t;
        return 1.0f - one_minus_t * one_minus_t * one_minus_t * one_minus_t;
    }

    /**
     * Quartic ease-in-out
     * f(t) = t < 0.5 ? 8*t^4 : 1 - 8*(1-t)^4
     */
    static float easeInOutQuart(float t)
    {
        if (t < 0.5f)
        {
            return 8.0f * t * t * t * t;
        }
        else
        {
            float one_minus_t = 1.0f - t;
            return 1.0f - 8.0f * one_minus_t * one_minus_t * one_minus_t * one_minus_t;
        }
    }

    // ========== Quintic (t^5) ==========

    /**
     * Quintic ease-in: slowest polynomial curve
     * f(t) = t^5
     */
    static float easeInQuint(float t)
    {
        return t * t * t * t * t;
    }

    /**
     * Quintic ease-out: smoothest polynomial deceleration
     * f(t) = 1 - (1-t)^5
     */
    static float easeOutQuint(float t)
    {
        float one_minus_t = 1.0f - t;
        return 1.0f - one_minus_t * one_minus_t * one_minus_t * one_minus_t * one_minus_t;
    }

    /**
     * Quintic ease-in-out
     * f(t) = t < 0.5 ? 16*t^5 : 1 - 16*(1-t)^5
     */
    static float easeInOutQuint(float t)
    {
        if (t < 0.5f)
        {
            return 16.0f * t * t * t * t * t;
        }
        else
        {
            float one_minus_t = 1.0f - t;
            return 1.0f - 16.0f * one_minus_t * one_minus_t * one_minus_t * one_minus_t * one_minus_t;
        }
    }

    // ========== Exponential ==========

    /**
     * Exponential ease-in: starts extremely slow
     * f(t) = 2^(10*(t-1))
     */
    static float easeInExpo(float t)
    {
        return t == 0.0f ? 0.0f : std::pow(2.0f, 10.0f * (t - 1.0f));
    }

    /**
     * Exponential ease-out: starts extremely fast
     * f(t) = 1 - 2^(-10*t)
     */
    static float easeOutExpo(float t)
    {
        return t == 1.0f ? 1.0f : 1.0f - std::pow(2.0f, -10.0f * t);
    }

    /**
     * Exponential ease-in-out
     * f(t) = smooth exponential curve
     */
    static float easeInOutExpo(float t)
    {
        if (t == 0.0f)
            return 0.0f;
        if (t == 1.0f)
            return 1.0f;

        if (t < 0.5f)
            return 0.5f * std::pow(2.0f, 20.0f * t - 10.0f);
        else
            return 1.0f - 0.5f * std::pow(2.0f, -20.0f * t + 10.0f);
    }

    // ========== Circular ==========

    /**
     * Circular ease-in: sqrt-based ease
     * f(t) = 1 - sqrt(1 - t^2)
     */
    static float easeInCirc(float t)
    {
        return 1.0f - std::sqrt(1.0f - t * t);
    }

    /**
     * Circular ease-out: smooth circle arc
     * f(t) = sqrt(1 - (1-t)^2)
     */
    static float easeOutCirc(float t)
    {
        return std::sqrt(1.0f - (1.0f - t) * (1.0f - t));
    }

    /**
     * Circular ease-in-out
     * f(t) = smooth circular curve
     */
    static float easeInOutCirc(float t)
    {
        if (t < 0.5f)
            return 0.5f * (1.0f - std::sqrt(1.0f - 4.0f * t * t));
        else
            return 0.5f * (std::sqrt(1.0f - 4.0f * (1.0f - t) * (1.0f - t)) + 1.0f);
    }

    // ========== Elastic ==========

    /**
     * Elastic ease-in: spring-like effect with overshoot
     * f(t) = oscillates with decreasing amplitude
     */
    static float easeInElastic(float t)
    {
        if (t == 0.0f)
            return 0.0f;
        if (t == 1.0f)
            return 1.0f;

        const float c4 = (2.0f * 3.14159265359f) / 3.0f;
        return -std::pow(2.0f, 10.0f * t - 10.0f) * std::sin((t * 10.0f - 10.75f) * c4);
    }

    /**
     * Elastic ease-out: spring-like oscillation
     * f(t) = oscillates with increasing amplitude
     */
    static float easeOutElastic(float t)
    {
        if (t == 0.0f)
            return 0.0f;
        if (t == 1.0f)
            return 1.0f;

        const float c4 = (2.0f * 3.14159265359f) / 3.0f;
        return std::pow(2.0f, -10.0f * t) * std::sin((t * 10.0f - 0.75f) * c4) + 1.0f;
    }

    /**
     * Elastic ease-in-out: double elastic effect
     * f(t) = smooth elastic curve
     */
    static float easeInOutElastic(float t)
    {
        if (t == 0.0f)
            return 0.0f;
        if (t == 1.0f)
            return 1.0f;

        const float c5 = (2.0f * 3.14159265359f) / 4.5f;

        if (t < 0.5f)
        {
            return -(std::pow(2.0f, 20.0f * t - 10.0f) * std::sin((20.0f * t - 11.125f) * c5)) / 2.0f;
        }
        else
        {
            return (std::pow(2.0f, -20.0f * t + 10.0f) * std::sin((20.0f * t - 11.125f) * c5)) / 2.0f + 1.0f;
        }
    }

    // ========== Bounce ==========

    /**
     * Bounce ease-out: bouncing effect
     * f(t) = decelerates with bounces
     */
    static float easeOutBounce(float t)
    {
        const float n1 = 7.5625f;
        const float d1 = 2.75f;

        if (t < 1.0f / d1)
        {
            return n1 * t * t;
        }
        else if (t < 2.0f / d1)
        {
            float adjusted = t - 1.5f / d1;
            return n1 * adjusted * adjusted + 0.75f;
        }
        else if (t < 2.5f / d1)
        {
            float adjusted = t - 2.25f / d1;
            return n1 * adjusted * adjusted + 0.9375f;
        }
        else
        {
            float adjusted = t - 2.625f / d1;
            return n1 * adjusted * adjusted + 0.984375f;
        }
    }

    /**
     * Bounce ease-in: bouncing at start
     * f(t) = 1 - easeOutBounce(1 - t)
     */
    static float easeInBounce(float t)
    {
        return 1.0f - easeOutBounce(1.0f - t);
    }

    /**
     * Bounce ease-in-out: bouncing at start and end
     * f(t) = smooth bouncing curve
     */
    static float easeInOutBounce(float t)
    {
        if (t < 0.5f)
            return (1.0f - easeOutBounce(1.0f - 2.0f * t)) / 2.0f;
        else
            return (1.0f + easeOutBounce(2.0f * t - 1.0f)) / 2.0f;
    }

    // ========== Back (overshoot) ==========

    /**
     * Back ease-in: pulls back slightly at start
     * f(t) = t^3 - t * sin(pi * t)
     */
    static float easeInBack(float t)
    {
        const float c1 = 1.70158f;
        const float c3 = c1 + 1.0f;
        return c3 * t * t * t - c1 * t * t;
    }

    /**
     * Back ease-out: overshoots slightly at end
     * f(t) = smooth overshoot
     */
    static float easeOutBack(float t)
    {
        const float c1 = 1.70158f;
        const float c3 = c1 + 1.0f;
        return 1.0f + c3 * std::pow(t - 1.0f, 3.0f) + c1 * std::pow(t - 1.0f, 2.0f);
    }

    /**
     * Back ease-in-out
     * f(t) = smooth overshoot curve
     */
    static float easeInOutBack(float t)
    {
        const float c1 = 1.70158f;
        const float c2 = c1 * 1.525f;

        if (t < 0.5f)
        {
            return (std::pow(2.0f * t, 2.0f) * ((c2 + 1.0f) * 2.0f * t - c2)) / 2.0f;
        }
        else
        {
            return (std::pow(2.0f * t - 2.0f, 2.0f) * ((c2 + 1.0f) * (t * 2.0f - 2.0f) + c2) + 2.0f) / 2.0f;
        }
    }

    // ========== Custom easing by name ==========

    /**
     * Create easing function from name string
     *
     * Supported names (case-insensitive):
     * - "linear"
     * - "easeInQuad", "easeOutQuad", "easeInOutQuad"
     * - "easeInCubic", "easeOutCubic", "easeInOutCubic"
     * - "easeInQuart", "easeOutQuart", "easeInOutQuart"
     * - "easeInQuint", "easeOutQuint", "easeInOutQuint"
     * - "easeInExpo", "easeOutExpo", "easeInOutExpo"
     * - "easeInCirc", "easeOutCirc", "easeInOutCirc"
     * - "easeInElastic", "easeOutElastic", "easeInOutElastic"
     * - "easeInBounce", "easeOutBounce", "easeInOutBounce"
     * - "easeInBack", "easeOutBack", "easeInOutBack"
     *
     * @param name Easing function name
     * @return Easing function, or linear if name not found
     */
    static EasingFunction create(const std::string& name)
    {
        // Normalize name (lowercase)
        std::string normalized = name;
        for (auto& c : normalized)
            c = std::tolower(c);

        if (normalized == "linear")
            return linear;

        // Quadratic
        if (normalized == "easeinquad")
            return easeInQuad;
        if (normalized == "easeoutquad")
            return easeOutQuad;
        if (normalized == "easeinoutquad")
            return easeInOutQuad;

        // Cubic
        if (normalized == "easeincubic")
            return easeInCubic;
        if (normalized == "easeoutcubic")
            return easeOutCubic;
        if (normalized == "easeinoutcubic")
            return easeInOutCubic;

        // Quartic
        if (normalized == "easeinquart")
            return easeInQuart;
        if (normalized == "easeoutquart")
            return easeOutQuart;
        if (normalized == "easeinoutquart")
            return easeInOutQuart;

        // Quintic
        if (normalized == "easeinquint")
            return easeInQuint;
        if (normalized == "easeoutquint")
            return easeOutQuint;
        if (normalized == "easeinoutquint")
            return easeInOutQuint;

        // Exponential
        if (normalized == "easeinexpo")
            return easeInExpo;
        if (normalized == "easeoutexpo")
            return easeOutExpo;
        if (normalized == "easeinoutexpo")
            return easeInOutExpo;

        // Circular
        if (normalized == "easeincirc")
            return easeInCirc;
        if (normalized == "easeoutcirc")
            return easeOutCirc;
        if (normalized == "easeinoutcirc")
            return easeInOutCirc;

        // Elastic
        if (normalized == "easeinelastic")
            return easeInElastic;
        if (normalized == "easeoutelastic")
            return easeOutElastic;
        if (normalized == "easeinoutelastic")
            return easeInOutElastic;

        // Bounce
        if (normalized == "easeinbounce")
            return easeInBounce;
        if (normalized == "easeoutbounce")
            return easeOutBounce;
        if (normalized == "easeinoutbounce")
            return easeInOutBounce;

        // Back
        if (normalized == "easeinback")
            return easeInBack;
        if (normalized == "easeoutback")
            return easeOutBack;
        if (normalized == "easeinoutback")
            return easeInOutBack;

        // Default to linear if unknown
        return linear;
    }
};

}  // namespace Animations
}  // namespace UI
}  // namespace Sidechain
