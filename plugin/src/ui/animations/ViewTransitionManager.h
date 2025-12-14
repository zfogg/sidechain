#pragma once

#include "TransitionAnimation.h"
#include "AnimationTimeline.h"
#include "Easing.h"
#include <JuceHeader.h>
#include <memory>
#include <functional>

namespace Sidechain {
namespace UI {
namespace Animations {

/**
 * ViewTransitionManager - Handles animated view transitions
 *
 * Manages transitions between views with support for various animation styles:
 * - Slide: View slides in from direction, previous slides out in opposite direction
 * - Fade: Views cross-fade (previous fades out, new fades in)
 * - Scale: View scales up from center while previous scales down
 * - Scale+Fade: Combine scale and fade for more dramatic effect
 *
 * Usage:
 *   auto manager = ViewTransitionManager::create(editor);
 *   manager->slideLeft(fromView, toView, 300);     // Slide with 300ms duration
 *   manager->fadeTransition(fromView, toView, 200);
 *   manager->scaleIn(fromView, toView, 400);
 *
 * The manager automatically handles component visibility, Z-order, and cleanup.
 */
class ViewTransitionManager : public std::enable_shared_from_this<ViewTransitionManager>
{
public:
    // Transition types
    enum class TransitionType
    {
        SlideLeft,    // View slides in from right, previous slides out left
        SlideRight,   // View slides in from left, previous slides out right
        SlideUp,      // View slides in from bottom, previous slides out up
        SlideDown,    // View slides in from top, previous slides out down
        Fade,         // Cross-fade transition
        ScaleIn,      // View scales up from center
        ScaleOut,     // View scales down to center
        ScaleFade     // Scale + fade combined
    };

    using TransitionCallback = std::function<void()>;

    /**
     * Create a new view transition manager
     *
     * @param editorComponent The parent component (usually the editor)
     */
    static std::shared_ptr<ViewTransitionManager> create(juce::Component* editorComponent)
    {
        return std::make_shared<ViewTransitionManager>(editorComponent);
    }

    explicit ViewTransitionManager(juce::Component* parent)
        : parentComponent_(parent)
        , isTransitioning_(false)
        , defaultDurationMs_(300)
    {
    }

    virtual ~ViewTransitionManager() = default;

    // ========== Configuration ==========

    /**
     * Set the default transition duration
     *
     * @param durationMs Duration in milliseconds
     */
    void setDefaultDuration(int durationMs)
    {
        defaultDurationMs_ = durationMs;
    }

    /**
     * Get the default transition duration
     */
    int getDefaultDuration() const { return defaultDurationMs_; }

    // ========== Transition Methods ==========

    /**
     * Slide transition - view slides in from right, previous slides left
     *
     * @param oldView The component to transition from
     * @param newView The component to transition to
     * @param durationMs Duration in milliseconds (0 = use default)
     * @param callback Invoked when transition completes
     */
    void slideLeft(juce::Component* oldView, juce::Component* newView, int durationMs = 0,
                   TransitionCallback callback = nullptr)
    {
        slideTransition(oldView, newView, -1, durationMs, callback);
    }

    /**
     * Slide transition - view slides in from left, previous slides right
     */
    void slideRight(juce::Component* oldView, juce::Component* newView, int durationMs = 0,
                    TransitionCallback callback = nullptr)
    {
        slideTransition(oldView, newView, 1, durationMs, callback);
    }

    /**
     * Slide transition - view slides in from bottom, previous slides up
     */
    void slideUp(juce::Component* oldView, juce::Component* newView, int durationMs = 0,
                 TransitionCallback callback = nullptr)
    {
        slideVerticalTransition(oldView, newView, -1, durationMs, callback);
    }

    /**
     * Slide transition - view slides in from top, previous slides down
     */
    void slideDown(juce::Component* oldView, juce::Component* newView, int durationMs = 0,
                   TransitionCallback callback = nullptr)
    {
        slideVerticalTransition(oldView, newView, 1, durationMs, callback);
    }

    /**
     * Fade transition - smooth cross-fade between views
     *
     * @param oldView The component to fade out
     * @param newView The component to fade in
     * @param durationMs Duration in milliseconds (0 = use default)
     * @param callback Invoked when transition completes
     */
    void fadeTransition(juce::Component* oldView, juce::Component* newView, int durationMs = 0,
                        TransitionCallback callback = nullptr)
    {
        int duration = durationMs > 0 ? durationMs : defaultDurationMs_;

        if (!parentComponent_)
            return;

        isTransitioning_ = true;

        // Show new view at full opacity
        newView->setVisible(true);
        newView->setAlpha(0.0f);

        // Animate old view fade out while new view fades in
        auto timeline = AnimationTimeline::parallel();

        // Fade out old view
        auto fadeOut = TransitionAnimation<float>::create(1.0f, 0.0f, duration)
            ->withEasing(Easing::easeOutQuad)
            ->onProgress([oldView](float alpha) {
                if (oldView)
                    oldView->setAlpha(alpha);
            });

        // Fade in new view
        auto fadeIn = TransitionAnimation<float>::create(0.0f, 1.0f, duration)
            ->withEasing(Easing::easeOutQuad)
            ->onProgress([newView](float alpha) {
                if (newView)
                    newView->setAlpha(alpha);
            });

        timeline->add(fadeOut)->add(fadeIn)->onCompletion([this, oldView, callback]() {
            if (oldView)
                oldView->setVisible(false);
            isTransitioning_ = false;
            if (callback)
                callback();
        });

        timeline->start();
    }

    /**
     * Scale transition - view scales up from center
     *
     * @param oldView The component to scale down
     * @param newView The component to scale up
     * @param durationMs Duration in milliseconds (0 = use default)
     * @param callback Invoked when transition completes
     */
    void scaleIn(juce::Component* oldView, juce::Component* newView, int durationMs = 0,
                 TransitionCallback callback = nullptr)
    {
        int duration = durationMs > 0 ? durationMs : defaultDurationMs_;

        if (!parentComponent_)
            return;

        isTransitioning_ = true;

        // Show new view with small scale
        newView->setVisible(true);
        newView->setAlpha(0.0f);

        // Animate scale and fade for new view, fade out old view
        auto timeline = AnimationTimeline::parallel();

        // Scale up new view from 0.8 to 1.0 while fading in
        auto scaleNewView = TransitionAnimation<float>::create(0.8f, 1.0f, duration)
            ->withEasing(Easing::easeOutCubic)
            ->onProgress([newView](float scale) {
                if (newView)
                {
                    auto bounds = newView->getBounds();
                    int centerX = bounds.getCentreX();
                    int centerY = bounds.getCentreY();
                    newView->setTransform(
                        juce::AffineTransform::scale(scale, scale, centerX, centerY));
                }
            });

        // Fade in new view
        auto fadeNewView = TransitionAnimation<float>::create(0.0f, 1.0f, duration)
            ->withEasing(Easing::easeOutQuad)
            ->onProgress([newView](float alpha) {
                if (newView)
                    newView->setAlpha(alpha);
            });

        // Fade out old view
        auto fadeOldView = TransitionAnimation<float>::create(1.0f, 0.0f, duration)
            ->withEasing(Easing::easeOutQuad)
            ->onProgress([oldView](float alpha) {
                if (oldView)
                    oldView->setAlpha(alpha);
            });

        timeline->add(scaleNewView)->add(fadeNewView)->add(fadeOldView)->onCompletion(
            [this, oldView, newView, callback]() {
                if (oldView)
                    oldView->setVisible(false);
                if (newView)
                    newView->setTransform(juce::AffineTransform());  // Reset transform
                isTransitioning_ = false;
                if (callback)
                    callback();
            });

        timeline->start();
    }

    /**
     * Scale out transition - view scales down to center while fading
     */
    void scaleOut(juce::Component* oldView, juce::Component* newView, int durationMs = 0,
                  TransitionCallback callback = nullptr)
    {
        int duration = durationMs > 0 ? durationMs : defaultDurationMs_;

        if (!parentComponent_)
            return;

        isTransitioning_ = true;

        // Show new view
        newView->setVisible(true);
        newView->setAlpha(0.0f);

        // Old view scales down while new fades in
        auto timeline = AnimationTimeline::parallel();

        // Scale down old view
        auto scaleOldView = TransitionAnimation<float>::create(1.0f, 0.8f, duration)
            ->withEasing(Easing::easeInCubic)
            ->onProgress([oldView](float scale) {
                if (oldView)
                {
                    auto bounds = oldView->getBounds();
                    int centerX = bounds.getCentreX();
                    int centerY = bounds.getCentreY();
                    oldView->setTransform(
                        juce::AffineTransform::scale(scale, scale, centerX, centerY));
                }
            });

        // Fade out old view
        auto fadeOldView = TransitionAnimation<float>::create(1.0f, 0.0f, duration)
            ->withEasing(Easing::easeOutQuad)
            ->onProgress([oldView](float alpha) {
                if (oldView)
                    oldView->setAlpha(alpha);
            });

        // Fade in new view
        auto fadeNewView = TransitionAnimation<float>::create(0.0f, 1.0f, duration)
            ->withEasing(Easing::easeOutQuad)
            ->onProgress([newView](float alpha) {
                if (newView)
                    newView->setAlpha(alpha);
            });

        timeline->add(scaleOldView)->add(fadeOldView)->add(fadeNewView)->onCompletion(
            [this, oldView, newView, callback]() {
                if (oldView)
                {
                    oldView->setVisible(false);
                    oldView->setTransform(juce::AffineTransform());
                }
                isTransitioning_ = false;
                if (callback)
                    callback();
            });

        timeline->start();
    }

    // ========== State Queries ==========

    /**
     * Check if a transition is currently in progress
     */
    bool isTransitioning() const { return isTransitioning_; }

    /**
     * Cancel any active transition
     */
    void cancelTransition()
    {
        isTransitioning_ = false;
        // In a real implementation, would cancel any active timelines
    }

private:
    juce::Component* parentComponent_;
    bool isTransitioning_;
    int defaultDurationMs_;

    /**
     * Internal horizontal slide transition
     *
     * @param direction -1 for left, 1 for right
     */
    void slideTransition(juce::Component* oldView, juce::Component* newView, int direction,
                         int durationMs, TransitionCallback callback)
    {
        int duration = durationMs > 0 ? durationMs : defaultDurationMs_;

        if (!parentComponent_)
            return;

        isTransitioning_ = true;

        auto parentWidth = parentComponent_->getWidth();

        // Show new view off-screen
        newView->setVisible(true);
        newView->setAlpha(1.0f);

        // Animate slide positions
        auto timeline = AnimationTimeline::parallel();

        // Slide new view in from the side
        auto slideNewView = TransitionAnimation<int>::create(-parentWidth * direction, 0, duration)
            ->withEasing(Easing::easeOutCubic)
            ->onProgress([newView, parentWidth, direction](int offset) {
                if (newView)
                {
                    auto bounds = newView->getBounds();
                    newView->setBounds(bounds.getX() + offset, bounds.getY(), bounds.getWidth(),
                                       bounds.getHeight());
                }
            });

        // Slide old view out to the opposite side
        auto slideOldView = TransitionAnimation<int>::create(0, parentWidth * direction, duration)
            ->withEasing(Easing::easeOutCubic)
            ->onProgress([oldView](int offset) {
                if (oldView)
                {
                    auto bounds = oldView->getBounds();
                    oldView->setBounds(bounds.getX() + offset, bounds.getY(),
                                       bounds.getWidth(), bounds.getHeight());
                }
            });

        timeline->add(slideNewView)->add(slideOldView)->onCompletion([this, oldView, callback]() {
            if (oldView)
                oldView->setVisible(false);
            isTransitioning_ = false;
            if (callback)
                callback();
        });

        timeline->start();
    }

    /**
     * Internal vertical slide transition
     *
     * @param direction -1 for up, 1 for down
     */
    void slideVerticalTransition(juce::Component* oldView, juce::Component* newView,
                                 int direction, int durationMs, TransitionCallback callback)
    {
        int duration = durationMs > 0 ? durationMs : defaultDurationMs_;

        if (!parentComponent_)
            return;

        isTransitioning_ = true;

        auto parentHeight = parentComponent_->getHeight();

        // Show new view off-screen
        newView->setVisible(true);
        newView->setAlpha(1.0f);

        // Animate slide positions
        auto timeline = AnimationTimeline::parallel();

        // Slide new view in from top/bottom
        auto slideNewView = TransitionAnimation<int>::create(-parentHeight * direction, 0, duration)
            ->withEasing(Easing::easeOutCubic)
            ->onProgress([newView](int offset) {
                if (newView)
                {
                    auto bounds = newView->getBounds();
                    newView->setBounds(bounds.getX(), bounds.getY() + offset,
                                       bounds.getWidth(), bounds.getHeight());
                }
            });

        // Slide old view out
        auto slideOldView = TransitionAnimation<int>::create(0, parentHeight * direction, duration)
            ->withEasing(Easing::easeOutCubic)
            ->onProgress([oldView](int offset) {
                if (oldView)
                {
                    auto bounds = oldView->getBounds();
                    oldView->setBounds(bounds.getX(), bounds.getY() + offset,
                                       bounds.getWidth(), bounds.getHeight());
                }
            });

        timeline->add(slideNewView)->add(slideOldView)->onCompletion([this, oldView, callback]() {
            if (oldView)
                oldView->setVisible(false);
            isTransitioning_ = false;
            if (callback)
                callback();
        });

        timeline->start();
    }

    // Prevent copying
    ViewTransitionManager(const ViewTransitionManager&) = delete;
    ViewTransitionManager& operator=(const ViewTransitionManager&) = delete;
};

}  // namespace Animations
}  // namespace UI
}  // namespace Sidechain
