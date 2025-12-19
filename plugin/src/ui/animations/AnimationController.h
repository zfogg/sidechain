#pragma once

#include "AnimationTimeline.h"
#include "TransitionAnimation.h"
#include "ViewTransitionManager.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <atomic>
#include <map>
#include <memory>
#include <vector>

namespace Sidechain {
namespace UI {
namespace Animations {

/**
 * AnimationHandle - Safe handle for managing animation lifecycle
 *
 * Prevents use-after-free by checking if the animation is still valid before cancellation.
 * Handles are automatically invalidated when animations complete or components are destroyed.
 *
 * Usage:
 *   auto handle = controller->fadeIn(component, 300);
 *   // Later, safely cancel even if component was destroyed:
 *   controller->cancel(handle);  // Safe - no-op if invalid
 */
class AnimationHandle {
  friend class AnimationController;

public:
  AnimationHandle() : id_(0) {}
  bool isValid() const {
    return id_ != 0;
  }
  uint64_t getId() const {
    return id_;
  }

private:
  explicit AnimationHandle(uint64_t id) : id_(id) {}
  uint64_t id_;
};

/**
 * AnimationController - Centralized animation lifecycle management
 *
 * Singleton that manages all animations in the plugin with:
 * - Central timer (single ~60fps update vs scattered timers)
 * - Handle-based cancellation (safe even if component deleted)
 * - Component-aware cleanup (auto-cancel animations when component destroyed)
 * - Animation presets (fadeIn, slideIn, scaleUp, etc.)
 * - Animation grouping and batch operations
 *
 * Key advantages:
 * - Reduced CPU overhead (one timer instead of many)
 * - Memory safety (handles prevent use-after-free)
 * - Automatic cleanup (no dangling animation references)
 * - Fluent API for common animation patterns
 *
 * Usage:
 *   auto &controller = AnimationController::getInstance();
 *   auto handle = controller.fadeIn(myComponent, 300);
 *   controller.onCompletion(handle, []() {
 *     // animation done
 *   });
 *   controller.cancel(handle);  // Safe cancellation
 *
 * Thread safety:
 *   - Main thread only (JUCE timer callback runs on message thread)
 *   - Call all methods from message thread
 */
class AnimationController : public juce::Timer {
public:
  // Get singleton instance
  static AnimationController &getInstance();

  // ========== Lifecycle ==========

  ~AnimationController();

  /**
   * Enable/disable all animations globally
   * Useful for testing or accessibility options
   */
  void setEnabled(bool enabled) {
    enabled_ = enabled;
  }
  bool isEnabled() const {
    return enabled_;
  }

  // ========== Generic Animation Scheduling ==========

  /**
   * Schedule a transition animation
   *
   * @param animation The animation to run
   * @param component Optional: component to track for cleanup
   * @return Handle for cancellation and callbacks
   */
  template <typename T>
  AnimationHandle schedule(std::shared_ptr<TransitionAnimation<T>> animation, juce::Component *component = nullptr) {
    if (!enabled_ || !animation) {
      return AnimationHandle();
    }

    uint64_t id = generateHandle();
    auto entry = std::make_shared<AnimationEntry>();
    entry->id = id;
    entry->animation = animation;
    entry->component = component;
    entry->timeline = nullptr;

    animations_[id] = entry;

    if (!isTimerRunning()) {
      startTimer(16); // ~60fps @ 16ms
    }

    animation->start();
    return AnimationHandle(id);
  }

  /**
   * Schedule an animation timeline
   *
   * @param timeline The timeline to run
   * @param component Optional: component to track for cleanup
   * @return Handle for cancellation and callbacks
   */
  AnimationHandle schedule(std::shared_ptr<AnimationTimeline> timeline, juce::Component *component = nullptr);

  // ========== Convenient Preset Animations ==========

  /**
   * Fade in component from alpha 0 to 1
   * @param component Component to fade in
   * @param durationMs Animation duration
   * @return Handle for cancellation/callbacks
   */
  AnimationHandle fadeIn(juce::Component *component, int durationMs = 300);

  /**
   * Fade out component from alpha 1 to 0
   */
  AnimationHandle fadeOut(juce::Component *component, int durationMs = 300);

  /**
   * Fade to specific alpha
   */
  AnimationHandle fadeTo(juce::Component *component, float alpha, int durationMs = 300);

  /**
   * Slide component in from left edge
   */
  AnimationHandle slideInFromLeft(juce::Component *component, int durationMs = 300);

  /**
   * Slide component in from right edge
   */
  AnimationHandle slideInFromRight(juce::Component *component, int durationMs = 300);

  /**
   * Slide component in from top edge
   */
  AnimationHandle slideInFromTop(juce::Component *component, int durationMs = 300);

  /**
   * Slide component in from bottom edge
   */
  AnimationHandle slideInFromBottom(juce::Component *component, int durationMs = 300);

  /**
   * Scale component from 0 to 1 (grow from center)
   */
  AnimationHandle scaleIn(juce::Component *component, int durationMs = 300);

  /**
   * Scale component from 1 to 0 (shrink to center)
   */
  AnimationHandle scaleOut(juce::Component *component, int durationMs = 300);

  /**
   * Scale to specific value
   */
  AnimationHandle scaleTo(juce::Component *component, float scale, int durationMs = 300);

  // ========== Animation Control ==========

  /**
   * Cancel animation by handle
   * Safe to call even if component was destroyed
   */
  void cancel(AnimationHandle handle);

  /**
   * Cancel all animations for a component
   * Useful when component is being destroyed
   */
  void cancelAllForComponent(juce::Component *component);

  /**
   * Pause animation by handle
   */
  void pause(AnimationHandle handle);

  /**
   * Resume animation by handle
   */
  void resume(AnimationHandle handle);

  /**
   * Pause all animations
   */
  void pauseAll();

  /**
   * Resume all animations
   */
  void resumeAll();

  /**
   * Cancel all running animations
   */
  void cancelAll();

  // ========== Callbacks ==========

  /**
   * Set callback when animation completes
   */
  void onCompletion(AnimationHandle handle, std::function<void()> callback);

  /**
   * Set callback when animation is cancelled
   */
  void onCancellation(AnimationHandle handle, std::function<void()> callback);

  /**
   * Set callback for progress updates
   * Progress value in range [0, 1]
   */
  void onProgress(AnimationHandle handle, std::function<void(float)> callback);

  // ========== State Queries ==========

  /**
   * Check if animation is running
   */
  bool isRunning(AnimationHandle handle) const;

  /**
   * Get number of active animations
   */
  size_t getActiveAnimationCount() const;

  /**
   * Get number of animations for component
   */
  size_t getAnimationCountForComponent(juce::Component *component) const;

  /**
   * Check if component has any running animations
   */
  bool hasAnimationsForComponent(juce::Component *component) const;

  /**
   * Get animation progress [0, 1]
   */
  float getProgress(AnimationHandle handle) const;

  // ========== Singleton Management ==========
  // Public constructor for make_unique compatibility - use getInstance instead
  AnimationController();

private:
  // Animation entry wrapper
  struct AnimationEntry {
    uint64_t id;
    std::shared_ptr<IAnimation> animation;       // Polymorphic animation interface
    std::shared_ptr<AnimationTimeline> timeline; // Optional timeline wrapper
    juce::Component *component;                  // Component to track for cleanup
    std::function<void()> completionCallback;
    std::function<void()> cancellationCallback;
    std::function<void(float)> progressCallback;
    bool isValid;

    AnimationEntry() : id(0), component(nullptr), isValid(true) {}
  };

  // JUCE Timer callback
  void timerCallback() override;

  // Helper to update all animations
  void updateAnimations();

  // Helper to cleanup finished animations
  void cleanupFinished();

  // Generate unique handle ID
  uint64_t generateHandle();

  // Create preset fade animation
  std::shared_ptr<TransitionAnimation<float>> createFadeAnimation(juce::Component *component, float startAlpha,
                                                                  float endAlpha, int durationMs);

  // Create preset slide animation
  std::shared_ptr<AnimationTimeline> createSlideAnimation(juce::Component *component, int startX, int startY, int endX,
                                                          int endY, int durationMs);

  // Create preset scale animation
  std::shared_ptr<TransitionAnimation<float>> createScaleAnimation(juce::Component *component, float startScale,
                                                                   float endScale, int durationMs);

  // Singleton instance
  static std::unique_ptr<AnimationController> instance_;
  static std::recursive_mutex instanceMutex_;

  // Animation storage
  std::map<uint64_t, std::shared_ptr<AnimationEntry>> animations_;
  std::atomic<uint64_t> nextHandleId_{1};

  // Global state
  bool enabled_;

  // Prevent copying
  AnimationController(const AnimationController &) = delete;
  AnimationController &operator=(const AnimationController &) = delete;
};

} // namespace Animations
} // namespace UI
} // namespace Sidechain
