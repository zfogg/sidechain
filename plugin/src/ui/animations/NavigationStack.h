#pragma once

#include "AnimationController.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <deque>
#include <functional>
#include <memory>

namespace Sidechain {
namespace UI {
namespace Animations {

/**
 * NavigationStack - Component-based view management with animated transitions
 *
 * Manages a stack of views/components with automatic animated transitions.
 * Provides a clean API for pushing/popping views with customizable entry/exit animations.
 *
 * Architecture:
 * - Stack-based view management (Last-In-First-Out navigation)
 * - Automatic animated transitions between views
 * - View lifecycle management (attach/detach from parent)
 * - Component ownership via unique_ptr
 * - Supports custom transition types and durations
 *
 * Usage:
 *   auto nav = std::make_unique<NavigationStack>(parentComponent);
 *
 *   // Push a new view with slide-in animation
 *   auto feedView = std::make_unique<PostsFeed>();
 *   nav->push(std::move(feedView), TransitionType::SlideInFromRight, 300);
 *
 *   // Pop current view with fade-out animation
 *   nav->pop(TransitionType::FadeOut, 200);
 *
 *   // Replace current view (pop then push)
 *   auto profileView = std::make_unique<Profile>();
 *   nav->replace(std::move(profileView), TransitionType::CrossFade, 300);
 *
 * Transition Types:
 * - Slide: SlideInFromLeft, SlideInFromRight, SlideInFromTop, SlideInFromBottom
 * - Fade: FadeIn, FadeOut, CrossFade
 * - Scale: ScaleIn, ScaleOut, ZoomIn, ZoomOut
 *
 * Thread safety: Main thread only (JUCE component operations)
 */
class NavigationStack : public juce::Component {
public:
  // Supported transition animations
  enum class TransitionType {
    // Slide transitions
    SlideInFromLeft,
    SlideInFromRight,
    SlideInFromTop,
    SlideInFromBottom,
    SlideOutToLeft,
    SlideOutToRight,
    SlideOutToTop,
    SlideOutToBottom,

    // Fade transitions
    FadeIn,
    FadeOut,
    CrossFade,

    // Scale transitions
    ScaleIn,
    ScaleOut,
    ZoomIn,
    ZoomOut,

    // Special: no animation
    Instant,
  };

  // Callback signature for navigation events
  using NavigationCallback = std::function<void(juce::Component *newView, juce::Component *previousView)>;

  /**
   * Create a navigation stack within a parent component
   *
   * @param parent Component that will contain all views
   * @param initialCapacity Pre-allocate stack capacity for efficiency
   */
  explicit NavigationStack(juce::Component *parent, size_t initialCapacity = 10);

  virtual ~NavigationStack() override;

  // ========== View Management ==========

  /**
   * Push a new view onto the stack with animated transition
   *
   * @param view The new view component to push
   * @param transition Type of transition animation
   * @param durationMs Animation duration in milliseconds
   * @param onComplete Callback when transition completes
   */
  void push(std::unique_ptr<juce::Component> view, TransitionType transition = TransitionType::SlideInFromRight,
            int durationMs = 300, NavigationCallback onComplete = nullptr);

  /**
   * Pop the current view with animated transition
   *
   * Removes the top view from stack and shows the previous view (if exists).
   *
   * @param transition Type of transition animation
   * @param durationMs Animation duration in milliseconds
   * @param onComplete Callback when transition completes
   * @return The popped view component (or nullptr if stack empty)
   */
  std::unique_ptr<juce::Component> pop(TransitionType transition = TransitionType::SlideOutToRight,
                                       int durationMs = 300, NavigationCallback onComplete = nullptr);

  /**
   * Replace the current view with a new view
   *
   * Equivalent to pop() followed by push() with automatic transition animation.
   *
   * @param view The new view component
   * @param transition Type of transition animation
   * @param durationMs Animation duration in milliseconds
   * @param onComplete Callback when transition completes
   */
  void replace(std::unique_ptr<juce::Component> view, TransitionType transition = TransitionType::CrossFade,
               int durationMs = 300, NavigationCallback onComplete = nullptr);

  /**
   * Pop all views except the root
   *
   * Clears the navigation stack back to the initial state.
   *
   * @param transition Type of transition animation for each pop
   * @param durationMs Animation duration per pop in milliseconds
   */
  void popToRoot(TransitionType transition = TransitionType::FadeOut, int durationMs = 200);

  /**
   * Clear all views from the stack
   */
  void clear();

  // ========== State Queries ==========

  /**
   * Get the current (top) view on the stack
   *
   * @return Pointer to current view, or nullptr if stack is empty
   */
  juce::Component *getCurrentView() const;

  /**
   * Get the previous view (one below the top)
   *
   * @return Pointer to previous view, or nullptr if fewer than 2 views
   */
  juce::Component *getPreviousView() const;

  /**
   * Get the number of views on the stack
   */
  size_t getStackDepth() const;

  /**
   * Check if stack is empty
   */
  bool isEmpty() const;

  /**
   * Check if navigation is currently animating
   */
  bool isAnimating() const;

  // ========== Configuration ==========

  /**
   * Set default transition type for push operations
   */
  void setDefaultPushTransition(TransitionType type, int durationMs = 300);

  /**
   * Set default transition type for pop operations
   */
  void setDefaultPopTransition(TransitionType type, int durationMs = 300);

  /**
   * Set callback invoked after any navigation
   */
  void setNavigationCallback(NavigationCallback callback);

  /**
   * Enable/disable transitions globally
   * Useful for testing or accessibility
   */
  void setTransitionsEnabled(bool enabled);

  // ========== JUCE Component Overrides ==========

  void resized() override;
  void paint(juce::Graphics &g) override {} // Views handle their own painting

private:
  // View entry on the stack
  struct StackEntry {
    std::unique_ptr<juce::Component> view;
    bool isVisible;

    StackEntry(std::unique_ptr<juce::Component> v) : view(std::move(v)), isVisible(true) {}
  };

  /**
   * Apply entry transition for pushing a new view
   */
  void applyEntryTransition(juce::Component *view, TransitionType type, int durationMs, NavigationCallback onComplete);

  /**
   * Apply exit transition for popping a view
   */
  void applyExitTransition(juce::Component *view, TransitionType type, int durationMs, NavigationCallback onComplete);

  /**
   * Show view and bring to front
   */
  void showView(juce::Component *view);

  /**
   * Hide view from display
   */
  void hideView(juce::Component *view);

  /**
   * Handle transition completion
   */
  void onTransitionComplete();

  // Stack storage
  std::deque<StackEntry> stack_;
  juce::Component *parent_;

  // Configuration
  TransitionType defaultPushTransition_;
  TransitionType defaultPopTransition_;
  int defaultPushDurationMs_;
  int defaultPopDurationMs_;
  NavigationCallback navigationCallback_;
  bool transitionsEnabled_;

  // Animation tracking
  int activeTransitionCount_;
  AnimationHandle currentTransitionHandle_;

  // Friend class for animation callbacks
  friend class AnimationController;
};

} // namespace Animations
} // namespace UI
} // namespace Sidechain
