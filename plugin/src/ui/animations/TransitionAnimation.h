#pragma once

#include "Easing.h"
#include <JuceHeader.h>
#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace Sidechain {
namespace UI {
namespace Animations {

/**
 * IAnimation - Common interface for all animations
 *
 * Provides basic control methods that all animation types must implement.
 */
class IAnimation {
public:
  virtual ~IAnimation() = default;

  virtual bool isRunning() const = 0;
  virtual bool isPaused() const = 0;
  virtual bool isSettled() const = 0;

  virtual void pause() = 0;
  virtual void resume() = 0;
  virtual void cancel() = 0;
};

/**
 * TransitionAnimation - Smooth value transitions with easing curves
 *
 * Animates a value from start to end over a specified duration with an easing
 * function. Supports progress callbacks and cancellation.
 *
 * Usage:
 *   auto anim = TransitionAnimation::create<float>(0.0f, 100.0f, 300)
 *       .withEasing(Easing::easeOutCubic)
 *       .onProgress([](float value) {
 *           component->setAlpha(value / 100.0f);
 *       })
 *       .start();
 *
 * @tparam T The animated value type (float, int, juce::Colour, etc)
 */
template <typename T>
class TransitionAnimation
    : public IAnimation,
      public std::enable_shared_from_this<TransitionAnimation<T>> {
public:
  using ProgressCallback = std::function<void(const T &)>;
  using CompletionCallback = std::function<void()>;
  using CancellationCallback = std::function<void()>;
  using EasingFunction = Easing::EasingFunction;
  using Interpolator =
      std::function<T(const T &start, const T &end, float progress)>;

  // ========== Lifecycle Methods ==========

  /**
   * Create a new animation
   *
   * @param startValue Initial value
   * @param endValue Final value
   * @param durationMs Duration in milliseconds
   */
  static std::shared_ptr<TransitionAnimation>
  create(const T &startValue, const T &endValue, int durationMs) {
    auto anim = std::shared_ptr<TransitionAnimation>(
        new TransitionAnimation(startValue, endValue, durationMs));
    return anim;
  }

  TransitionAnimation(const T &startValue, const T &endValue, int durationMs)
      : start_(startValue), end_(endValue), duration_(durationMs),
        easing_(Easing::easeOutCubic), isRunning_(false), isPaused_(false),
        startTime_(std::chrono::steady_clock::now()),
        pauseTime_(std::chrono::steady_clock::now()), pausedElapsed_(0) {
    // Default linear interpolator for float/int types - will be specialized
    interpolator_ = [](const T &start, const T &end, float progress) {
      return linearInterpolate(start, end, progress);
    };
  }

  ~TransitionAnimation() override = default;

  // ========== Configuration ==========

  /**
   * Set the easing function to use
   */
  std::shared_ptr<TransitionAnimation> withEasing(EasingFunction easing) {
    easing_ = easing;
    return this->shared_from_this();
  }

  /**
   * Set a custom interpolation function
   *
   * Called with (startValue, endValue, progress [0,1]) to compute current value
   */
  std::shared_ptr<TransitionAnimation>
  withInterpolator(Interpolator interpolator) {
    interpolator_ = interpolator;
    return this->shared_from_this();
  }

  /**
   * Register a callback invoked on each frame update
   *
   * @param callback Invoked with current animated value
   */
  std::shared_ptr<TransitionAnimation> onProgress(ProgressCallback callback) {
    if (callback)
      progressCallbacks_.push_back(callback);
    return this->shared_from_this();
  }

  /**
   * Register a callback invoked when animation completes
   *
   * @param callback Invoked when animation finishes naturally
   */
  std::shared_ptr<TransitionAnimation>
  onCompletion(CompletionCallback callback) {
    completionCallback_ = callback;
    return this->shared_from_this();
  }

  /**
   * Alias for onCompletion (shorter name)
   */
  std::shared_ptr<TransitionAnimation> onComplete(CompletionCallback callback) {
    return onCompletion(callback);
  }

  /**
   * Register a callback invoked when animation is cancelled
   *
   * @param callback Invoked when cancelled via cancel()
   */
  std::shared_ptr<TransitionAnimation>
  onCancellation(CancellationCallback callback) {
    cancellationCallback_ = callback;
    return this->shared_from_this();
  }

  // ========== Control ==========

  /**
   * Start the animation
   *
   * @return Shared pointer for chaining
   */
  std::shared_ptr<TransitionAnimation> start() {
    if (isRunning_)
      return this->shared_from_this();

    isRunning_ = true;
    isPaused_ = false;
    startTime_ = std::chrono::steady_clock::now();
    pausedElapsed_ = 0;

    // Start update timer
    timer_ = std::make_unique<AnimationTimer>(this);
    timer_->startTimer(16); // ~60fps @ 16ms per frame

    return this->shared_from_this();
  }

  /**
   * Pause the animation
   */
  void pause() override {
    if (isRunning_ && !isPaused_) {
      isPaused_ = true;
      pauseTime_ = std::chrono::steady_clock::now();
    }
  }

  /**
   * Resume a paused animation
   */
  void resume() override {
    if (isRunning_ && isPaused_) {
      isPaused_ = false;
      auto pauseDuration = std::chrono::steady_clock::now() - pauseTime_;
      pausedElapsed_ +=
          std::chrono::duration_cast<std::chrono::milliseconds>(pauseDuration)
              .count();
    }
  }

  /**
   * Cancel the animation
   */
  void cancel() override {
    if (isRunning_) {
      isRunning_ = false;
      stopTimer();

      if (cancellationCallback_)
        cancellationCallback_();
    }
  }

  // ========== State Queries ==========

  /**
   * Check if animation is currently running
   */
  bool isRunning() const override { return isRunning_; }

  /**
   * Check if animation is paused
   */
  bool isPaused() const override { return isPaused_; }

  /**
   * Check if animation is settled (completed)
   */
  bool isSettled() const override { return !isRunning_; }

  /**
   * Get current progress [0, 1]
   */
  float getProgress() const {
    auto elapsed = getElapsedTime();
    if (elapsed >= duration_)
      return 1.0f;
    return static_cast<float>(elapsed) / static_cast<float>(duration_);
  }

  /**
   * Get current animated value
   */
  T getCurrentValue() const {
    float progress = getProgress();
    float eased = easing_(progress);
    return interpolator_(start_, end_, eased);
  }

  /**
   * Get elapsed time in milliseconds
   */
  int getElapsedTime() const {
    if (!isRunning_)
      return 0;

    auto now = std::chrono::steady_clock::now();
    auto elapsed =
        std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime_);
    int elapsedMs = static_cast<int>(elapsed.count()) - pausedElapsed_;
    return std::max(0, elapsedMs);
  }

  /**
   * Get remaining time until completion (in ms)
   */
  int getRemainingTime() const {
    int elapsed = getElapsedTime();
    return std::max(0, duration_ - elapsed);
  }

  /**
   * Get animation duration in milliseconds
   */
  int getDuration() const { return duration_; }

  /**
   * Get start value
   */
  const T &getStartValue() const { return start_; }

  /**
   * Get end value
   */
  const T &getEndValue() const { return end_; }

private:
  // Inner timer class that inherits from juce::Timer
  class AnimationTimer : public juce::Timer {
  public:
    AnimationTimer(TransitionAnimation *parent) : parent_(parent) {}

    void timerCallback() override {
      if (parent_)
        parent_->updateFrame();
    }

  private:
    TransitionAnimation *parent_;
  };

  T start_;
  T end_;
  int duration_;
  EasingFunction easing_;
  Interpolator interpolator_;
  std::vector<ProgressCallback> progressCallbacks_;
  CompletionCallback completionCallback_;
  CancellationCallback cancellationCallback_;

  bool isRunning_;
  bool isPaused_;
  std::chrono::steady_clock::time_point startTime_;
  std::chrono::steady_clock::time_point pauseTime_;
  int pausedElapsed_;

  std::unique_ptr<AnimationTimer> timer_;

  /**
   * Called on each frame to update animation progress
   */
  void updateFrame() {
    if (!isRunning_ || isPaused_)
      return;

    int elapsed = getElapsedTime();

    if (elapsed >= duration_) {
      // Animation complete
      isRunning_ = false;
      stopTimer();

      // Call progress callback with final value
      T finalValue = interpolator_(start_, end_, 1.0f);
      for (auto &cb : progressCallbacks_)
        if (cb)
          cb(finalValue);

      // Call completion callback
      if (completionCallback_)
        completionCallback_();
    } else {
      // Animation in progress
      float progress =
          static_cast<float>(elapsed) / static_cast<float>(duration_);
      float eased = easing_(progress);
      T currentValue = interpolator_(start_, end_, eased);

      // Call progress callbacks
      for (auto &cb : progressCallbacks_)
        if (cb)
          cb(currentValue);
    }
  }

  /**
   * Stop the update timer
   */
  void stopTimer() {
    if (timer_) {
      timer_->stopTimer();
      timer_.reset();
    }
  }

  /**
   * Default linear interpolation for numeric types
   */
  static T linearInterpolate(const T &start, const T &end, float progress) {
    // This will be specialized for float, int, juce::Colour, etc.
    return static_cast<T>(start + (end - start) * progress);
  }

  // Prevent copying
  TransitionAnimation(const TransitionAnimation &) = delete;
  TransitionAnimation &operator=(const TransitionAnimation &) = delete;
};

// ========== Specializations for Common Types ==========

/**
 * Specialization for float animations
 */
template <>
inline float TransitionAnimation<float>::linearInterpolate(const float &start,
                                                           const float &end,
                                                           float progress) {
  return start + (end - start) * progress;
}

/**
 * Specialization for int animations
 */
template <>
inline int TransitionAnimation<int>::linearInterpolate(const int &start,
                                                       const int &end,
                                                       float progress) {
  return static_cast<int>(static_cast<float>(start) +
                          static_cast<float>(end - start) * progress);
}

/**
 * Specialization for juce::Colour animations
 */
template <>
inline juce::Colour TransitionAnimation<juce::Colour>::linearInterpolate(
    const juce::Colour &start, const juce::Colour &end, float progress) {
  return start.interpolatedWith(end, progress);
}

/**
 * Specialization for juce::Point<float> animations
 */
template <>
inline juce::Point<float>
TransitionAnimation<juce::Point<float>>::linearInterpolate(
    const juce::Point<float> &start, const juce::Point<float> &end,
    float progress) {
  return juce::Point<float>(start.x + (end.x - start.x) * progress,
                            start.y + (end.y - start.y) * progress);
}

/**
 * Specialization for juce::Rectangle<float> animations
 */
template <>
inline juce::Rectangle<float>
TransitionAnimation<juce::Rectangle<float>>::linearInterpolate(
    const juce::Rectangle<float> &start, const juce::Rectangle<float> &end,
    float progress) {
  return juce::Rectangle<float>(
      start.getX() + (end.getX() - start.getX()) * progress,
      start.getY() + (end.getY() - start.getY()) * progress,
      start.getWidth() + (end.getWidth() - start.getWidth()) * progress,
      start.getHeight() + (end.getHeight() - start.getHeight()) * progress);
}

// ========== Builder Helper for Cleaner Syntax ==========

/**
 * AnimationBuilder - Fluent API for creating animations
 *
 * Usage:
 *   AnimationBuilder<float>()
 *       .from(0.0f).to(100.0f)
 *       .duration(300)
 *       .easing(Easing::easeOutCubic)
 *       .onProgress([](float v) { // update logic })
 *       .start();
 */
template <typename T> class AnimationBuilder {
public:
  AnimationBuilder() : start_(), end_(), duration_(300) {}

  AnimationBuilder &from(const T &startValue) {
    start_ = startValue;
    hasStart_ = true;
    return *this;
  }

  AnimationBuilder &to(const T &endValue) {
    end_ = endValue;
    hasEnd_ = true;
    return *this;
  }

  AnimationBuilder &duration(int durationMs) {
    duration_ = durationMs;
    return *this;
  }

  AnimationBuilder &easing(Easing::EasingFunction easingFunc) {
    easing_ = easingFunc;
    return *this;
  }

  AnimationBuilder &
  onProgress(typename TransitionAnimation<T>::ProgressCallback callback) {
    progressCallback_ = callback;
    return *this;
  }

  AnimationBuilder &
  onCompletion(typename TransitionAnimation<T>::CompletionCallback callback) {
    completionCallback_ = callback;
    return *this;
  }

  std::shared_ptr<TransitionAnimation<T>> build() {
    if (!hasStart_ || !hasEnd_)
      throw std::runtime_error(
          "AnimationBuilder: must set both start and end values");

    auto anim = TransitionAnimation<T>::create(start_, end_, duration_);

    if (easing_)
      anim->withEasing(easing_);

    if (progressCallback_)
      anim->onProgress(progressCallback_);

    if (completionCallback_)
      anim->onCompletion(completionCallback_);

    return anim;
  }

  std::shared_ptr<TransitionAnimation<T>> start() { return build()->start(); }

private:
  T start_;
  T end_;
  int duration_;
  bool hasStart_ = false;
  bool hasEnd_ = false;
  Easing::EasingFunction easing_;
  typename TransitionAnimation<T>::ProgressCallback progressCallback_;
  typename TransitionAnimation<T>::CompletionCallback completionCallback_;
};

} // namespace Animations
} // namespace UI
} // namespace Sidechain
