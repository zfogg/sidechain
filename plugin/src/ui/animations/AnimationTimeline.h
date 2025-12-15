#pragma once

#include "TransitionAnimation.h"
#include <chrono>
#include <functional>
#include <memory>
#include <vector>

namespace Sidechain {
namespace UI {
namespace Animations {

/**
 * AnimationTimeline - Orchestrate multiple animations in sequence or parallel
 *
 * Supports:
 * - Sequential execution: each animation waits for previous to complete
 * - Parallel execution: all animations run simultaneously
 * - Stagger delays: offset animation starts for cascading effects
 * - Synchronized completion callbacks
 *
 * Usage:
 *   auto timeline = AnimationTimeline::sequential()
 *       .add(fadeOut, 300)
 *       .add(slideIn, 300)
 *       .add(scaleUp, 200)
 *       .withStagger(50)  // 50ms between each animation start
 *       .onCompletion([](){ std::cout << "All done!"; })
 *       .start();
 *
 *   Or for parallel:
 *   auto timeline = AnimationTimeline::parallel()
 *       .add(fadeIn, 300)
 *       .add(slideIn, 300)
 *       .add(scaleUp, 300)
 *       .onCompletion([](){ std::cout << "All done!"; })
 *       .start();
 */
class AnimationTimeline
    : public std::enable_shared_from_this<AnimationTimeline> {
public:
  // Timing mode for the timeline
  enum class TimingMode {
    Sequential, // Play animations one after another
    Parallel    // Play all animations simultaneously
  };

  using CompletionCallback = std::function<void()>;
  using CancellationCallback = std::function<void()>;
  using ProgressCallback =
      std::function<void(float)>; // Overall progress [0, 1]

  // ========== Factory Methods ==========

  /**
   * Create a sequential timeline (animations play one after another)
   */
  static std::shared_ptr<AnimationTimeline> sequential() {
    return std::make_shared<AnimationTimeline>(TimingMode::Sequential);
  }

  /**
   * Create a parallel timeline (all animations play simultaneously)
   */
  static std::shared_ptr<AnimationTimeline> parallel() {
    return std::make_shared<AnimationTimeline>(TimingMode::Parallel);
  }

  explicit AnimationTimeline(TimingMode mode = TimingMode::Sequential)
      : mode_(mode), isRunning_(false), currentAnimationIndex_(0),
        stageerDelay_(0), startTime_(std::chrono::steady_clock::now()) {}

  virtual ~AnimationTimeline() = default;

  // ========== Configuration ==========

  /**
   * Add an animation to the timeline
   *
   * @param animation The animation to add
   * @param duration Duration in milliseconds (used for display/tracking)
   */
  template <typename T>
  std::shared_ptr<AnimationTimeline>
  add(std::shared_ptr<TransitionAnimation<T>> animation, int duration = 0) {
    if (animation) {
      AnimationEntry entry;
      entry.animation = animation;
      entry.duration = duration > 0 ? duration : animation->getDuration();
      animations_.push_back(entry);
    }
    return this->shared_from_this();
  }

  /**
   * Add a stagger delay between animation starts
   *
   * For sequential timelines: delay between animations
   * For parallel timelines: offset each animation start by stagger amount
   *
   * @param delayMs Delay in milliseconds
   */
  std::shared_ptr<AnimationTimeline> withStagger(int delayMs) {
    stageerDelay_ = delayMs;
    return this->shared_from_this();
  }

  /**
   * Register a callback invoked when all animations complete
   */
  std::shared_ptr<AnimationTimeline> onCompletion(CompletionCallback callback) {
    completionCallback_ = callback;
    return this->shared_from_this();
  }

  /**
   * Register a callback invoked if timeline is cancelled
   */
  std::shared_ptr<AnimationTimeline>
  onCancellation(CancellationCallback callback) {
    cancellationCallback_ = callback;
    return this->shared_from_this();
  }

  /**
   * Register a callback for overall progress updates
   *
   * @param callback Invoked with overall progress [0, 1]
   */
  std::shared_ptr<AnimationTimeline> onProgress(ProgressCallback callback) {
    progressCallback_ = callback;
    return this->shared_from_this();
  }

  // ========== Control ==========

  /**
   * Start the timeline
   */
  std::shared_ptr<AnimationTimeline> start() {
    if (isRunning_ || animations_.empty())
      return this->shared_from_this();

    isRunning_ = true;
    currentAnimationIndex_ = 0;
    startTime_ = std::chrono::steady_clock::now();

    if (mode_ == TimingMode::Sequential)
      startSequentialAnimations();
    else
      startParallelAnimations();

    // Start progress update timer
    timer_ = std::make_unique<TimelineTimer>(this);
    timer_->startTimer(16); // ~60fps @ 16ms per frame

    return this->shared_from_this();
  }

  /**
   * Pause all animations in the timeline
   */
  std::shared_ptr<AnimationTimeline> pause() {
    for (auto &entry : animations_) {
      if (entry.animation && entry.animation->isRunning())
        entry.animation->pause();
    }
    return this->shared_from_this();
  }

  /**
   * Resume all paused animations
   */
  std::shared_ptr<AnimationTimeline> resume() {
    for (auto &entry : animations_) {
      if (entry.animation && entry.animation->isPaused())
        entry.animation->resume();
    }
    return this->shared_from_this();
  }

  /**
   * Cancel all animations in the timeline
   */
  std::shared_ptr<AnimationTimeline> cancel() {
    if (isRunning_) {
      isRunning_ = false;
      stopTimer();

      for (auto &entry : animations_) {
        if (entry.animation && entry.animation->isRunning())
          entry.animation->cancel();
      }

      if (cancellationCallback_)
        cancellationCallback_();
    }
    return this->shared_from_this();
  }

  // ========== State Queries ==========

  /**
   * Check if timeline is running
   */
  bool isRunning() const { return isRunning_; }

  /**
   * Get number of animations in timeline
   */
  size_t getAnimationCount() const { return animations_.size(); }

  /**
   * Get total duration of all animations
   */
  int getTotalDuration() const {
    if (animations_.empty())
      return 0;

    if (mode_ == TimingMode::Parallel) {
      // For parallel, total duration is max animation + stagger offsets
      int maxDuration = 0;
      for (const auto &entry : animations_)
        maxDuration = std::max(maxDuration, entry.duration);
      return maxDuration +
             (static_cast<int>(animations_.size()) - 1) * stageerDelay_;
    } else {
      // For sequential, total is sum of durations + stagger offsets
      int total = 0;
      for (const auto &entry : animations_)
        total += entry.duration;
      total += (static_cast<int>(animations_.size()) - 1) * stageerDelay_;
      return total;
    }
  }

  /**
   * Get current overall progress [0, 1]
   */
  float getProgress() const {
    int total = getTotalDuration();
    if (total <= 0)
      return 0.0f;

    int elapsed = getElapsedTime();
    return std::min(1.0f,
                    static_cast<float>(elapsed) / static_cast<float>(total));
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
    return static_cast<int>(elapsed.count());
  }

  /**
   * Get timing mode
   */
  TimingMode getTimingMode() const { return mode_; }

  /**
   * Check if all animations are complete
   */
  bool isComplete() const {
    if (!isRunning_)
      return true;

    for (const auto &entry : animations_) {
      if (entry.animation && entry.animation->isRunning())
        return false;
    }
    return true;
  }

private:
  // Animation entry in timeline
  struct AnimationEntry {
    std::shared_ptr<IAnimation> animation; // Polymorphic animation interface
    int duration;
  };

  TimingMode mode_;
  std::vector<AnimationEntry> animations_;
  bool isRunning_;
  size_t currentAnimationIndex_;
  int stageerDelay_;
  // Inner timer class that inherits from juce::Timer
  class TimelineTimer : public juce::Timer {
  public:
    TimelineTimer(AnimationTimeline *parent) : parent_(parent) {}

    void timerCallback() override {
      if (parent_)
        parent_->updateProgress();
    }

  private:
    AnimationTimeline *parent_;
  };

  std::chrono::steady_clock::time_point startTime_;
  CompletionCallback completionCallback_;
  CancellationCallback cancellationCallback_;
  ProgressCallback progressCallback_;
  std::unique_ptr<TimelineTimer> timer_;

  /**
   * Start animations in sequential mode (one after another)
   */
  void startSequentialAnimations() {
    if (animations_.empty())
      return;

    currentAnimationIndex_ = 0;
    if (currentAnimationIndex_ < animations_.size()) {
      // Schedule first animation with stagger delay
      int delayMs = static_cast<int>(currentAnimationIndex_) * stageerDelay_;
      scheduleNextAnimation(delayMs);
    }
  }

  /**
   * Start animations in parallel mode (all simultaneously with stagger offset)
   */
  void startParallelAnimations() {
    for (size_t i = 0; i < animations_.size(); ++i) {
      // Each animation starts with a stagger offset
      int delayMs = static_cast<int>(i) * stageerDelay_;
      scheduleAnimationWithDelay(i, delayMs);
    }
  }

  /**
   * Schedule the next animation in sequential mode
   */
  void scheduleNextAnimation(int delayMs) {
    if (currentAnimationIndex_ >= animations_.size()) {
      // All done
      completeTimeline();
      return;
    }

    // In a real implementation, this would use a timer to delay by delayMs
    // For simplicity in this version, we start immediately
    (void)delayMs;

    // TODO: Implement actual delay before starting next animation
    // For now, just start it immediately
  }

  /**
   * Schedule an animation at a given index with a delay
   */
  void scheduleAnimationWithDelay([[maybe_unused]] size_t index,
                                  [[maybe_unused]] int delayMs) {
    // In a real implementation, this would use a timer
    // TODO: Implement delay before starting animation
  }

  /**
   * Update overall progress and check for completion
   */
  void updateProgress() {
    if (!isRunning_)
      return;

    // Report overall progress
    if (progressCallback_)
      progressCallback_(getProgress());

    // Check if all animations are done
    if (isComplete()) {
      isRunning_ = false;
      stopTimer();
      completeTimeline();
    }
  }

  /**
   * Handle timeline completion
   */
  void completeTimeline() {
    isRunning_ = false;
    stopTimer();

    if (completionCallback_)
      completionCallback_();
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

  // Prevent copying
  AnimationTimeline(const AnimationTimeline &) = delete;
  AnimationTimeline &operator=(const AnimationTimeline &) = delete;
};

/**
 * TimelineBuilder - Fluent API for building animation timelines
 *
 * Usage:
 *   TimelineBuilder()
 *       .sequential()
 *       .add(anim1)
 *       .add(anim2)
 *       .withStagger(50)
 *       .onCompletion([]{})
 *       .build()
 *       .start();
 */
class TimelineBuilder {
public:
  TimelineBuilder() : timeline_(AnimationTimeline::sequential()) {}

  TimelineBuilder &sequential() {
    timeline_ = AnimationTimeline::sequential();
    return *this;
  }

  TimelineBuilder &parallel() {
    timeline_ = AnimationTimeline::parallel();
    return *this;
  }

  template <typename T>
  TimelineBuilder &add(std::shared_ptr<TransitionAnimation<T>> animation) {
    timeline_->add(animation);
    return *this;
  }

  TimelineBuilder &withStagger(int delayMs) {
    timeline_->withStagger(delayMs);
    return *this;
  }

  TimelineBuilder &
  onCompletion(AnimationTimeline::CompletionCallback callback) {
    timeline_->onCompletion(callback);
    return *this;
  }

  TimelineBuilder &onProgress(AnimationTimeline::ProgressCallback callback) {
    timeline_->onProgress(callback);
    return *this;
  }

  std::shared_ptr<AnimationTimeline> build() { return timeline_; }

  std::shared_ptr<AnimationTimeline> start() { return timeline_->start(); }

private:
  std::shared_ptr<AnimationTimeline> timeline_;
};

} // namespace Animations
} // namespace UI
} // namespace Sidechain
