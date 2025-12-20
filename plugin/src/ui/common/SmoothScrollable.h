#pragma once

#include "../../ui/animations/AnimationController.h"
#include "../../util/Log.h"
#include <JuceHeader.h>

namespace Sidechain::UI {

/**
 * SmoothScrollable provides reusable smooth scroll animation functionality
 *
 * Components that need scrolling can inherit from this class to get:
 * - Smooth 200ms scroll animations on mouse wheel
 * - Automatic animation cancellation on direct scrollbar manipulation
 * - Consistent scroll behavior across all scrollable views
 * - ScrollBar::Listener implementation
 *
 * Usage:
 *   class MyComponent : public juce::Component,
 *                       public SmoothScrollable {
 *   protected:
 *     void onScrollUpdate(double newScrollPosition) override {
 *       // Handle scroll position change
 *       repaint();
 *     }
 *   };
 *
 *   // In MyComponent::resized:
 *   setScrollBar(&myScrollBar);  // Register the scrollbar
 *
 * Subclasses should:
 * 1. Call setScrollBar() in resized() to register the scrollbar
 * 2. Override onScrollUpdate() to handle scroll changes
 * 3. Call handleMouseWheelMove() from their mouseWheelMove()
 * 4. No need to implement scrollBarMoved() - SmoothScrollable handles it!
 */
class SmoothScrollable : public juce::ScrollBar::Listener {
public:
  virtual ~SmoothScrollable() override = default;

  // ==============================================================================
  // Scroll state access
  double getScrollPosition() const {
    return scrollPosition;
  }
  double getTargetScrollPosition() const {
    return targetScrollPosition;
  }

  // ==============================================================================
  // Called by component's mouseWheelMove
  void handleMouseWheelMove(const juce::MouseEvent &event, const juce::MouseWheelDetails &wheel,
                            [[maybe_unused]] int viewportHeight, int scrollBarWidth) {
    if (!scrollBar)
      return;

    // Only scroll if wheel is within scrollable area (not over scroll bar)
    if (event.x >= getScrollableWidth(scrollBarWidth))
      return;

    double scrollAmount = wheel.deltaY * 100.0;
    double maxScrollPos = scrollBar->getMaximumRangeLimit();
    targetScrollPosition = juce::jlimit(0.0, maxScrollPos, scrollPosition - scrollAmount);

    // Cancel any existing animation
    if (scrollAnimationHandle.isValid()) {
      Sidechain::UI::Animations::AnimationController::getInstance().cancel(scrollAnimationHandle);
    }

    // Instant scroll - no animation for responsive feel
    scrollPosition = targetScrollPosition;
    if (scrollBar) {
      scrollBar->setCurrentRangeStart(scrollPosition, juce::dontSendNotification);
    }
    onScrollUpdate(scrollPosition);
  }

  // ==============================================================================
  // juce::ScrollBar::Listener implementation
  void scrollBarMoved(juce::ScrollBar *scrollBarPtr, double newRangeStart) override {
    if (scrollBarPtr != scrollBar)
      return;

    // Cancel any active animation since user is directly manipulating scrollbar
    if (scrollAnimationHandle.isValid()) {
      Sidechain::UI::Animations::AnimationController::getInstance().cancel(scrollAnimationHandle);
    }
    scrollPosition = newRangeStart;
    targetScrollPosition = newRangeStart;
    onScrollUpdate(scrollPosition);
  }

  // ==============================================================================
  // Set up scrollbar (call from component's resized)
  void setScrollBar(juce::ScrollBar *bar) {
    scrollBar = bar;
    if (scrollBar) {
      scrollBar->addListener(this);
    }
  }

protected:
  // ==============================================================================
  // Override this method to handle scroll position changes
  // Called whenever scroll position changes (during animation or direct manipulation)
  virtual void onScrollUpdate(double newScrollPosition) = 0;

  // ==============================================================================
  // Helper to get component name for logging
  virtual juce::String getComponentName() const {
    return "SmoothScrollable";
  }

  // ==============================================================================
  // Helper to get scrollable area width (total width - scrollbar width)
  // Override in derived class to return juce::Component::getWidth - scrollBarWidth
  virtual int getScrollableWidth([[maybe_unused]] int scrollBarWidth) const {
    // Default implementation: override in derived classes that inherit from juce::Component
    // Example: return Component::getWidth - scrollBarWidth;
    return 1388; // Keep original default for backward compatibility
  }

private:
  // Scroll state
  double scrollPosition = 0.0;
  double targetScrollPosition = 0.0;
  Sidechain::UI::Animations::AnimationHandle scrollAnimationHandle;
  juce::ScrollBar *scrollBar = nullptr; // Non-owning pointer
};

} // namespace Sidechain::UI
