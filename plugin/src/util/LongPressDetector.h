#pragma once

#include <JuceHeader.h>
#include <functional>

// ==============================================================================
/**
 * LongPressDetector - Long-press gesture detection
 *
 * Detects long-press gestures (press and hold) with configurable threshold.
 * Useful for context menus, emoji reactions, drag-to-delete, etc.
 *
 * Usage:
 *   LongPressDetector longPress;
 *   longPress.setThreshold(500);  // 500ms hold
 *   longPress.onLongPress = [this]() { showContextMenu(); };
 *
 *   void mouseDown(const MouseEvent&) { longPress.start(); }
 *   void mouseUp(const MouseEvent&) {
 *       if (!longPress.wasTriggered()) handleClick();
 *       longPress.cancel();
 *   }
 */
class LongPressDetector : private juce::Timer {
public:
  /**
   * Create detector with threshold duration.
   * @param thresholdMs Time in ms before long-press triggers (default 500ms)
   */
  explicit LongPressDetector(int thresholdMs = 500) : threshold(thresholdMs) {}

  ~LongPressDetector() override {
    stopTimer();
  }

  // ==========================================================================
  // Control

  /**
   * Start detecting a long-press.
   * Call this from mouseDown.
   */
  void start() {
    triggered = false;
    startTimer(threshold);
  }

  /**
   * Start with a specific callback.
   */
  void start(std::function<void()> callback) {
    onLongPress = std::move(callback);
    start();
  }

  /**
   * Cancel detection.
   * Call this from mouseUp or mouseExit.
   */
  void cancel() {
    stopTimer();
  }

  /**
   * Cancel and reset triggered state.
   */
  void reset() {
    cancel();
    triggered = false;
  }

  // ==========================================================================
  // State

  /**
   * Check if currently detecting (timer running).
   */
  bool isActive() const {
    return isTimerRunning();
  }

  /**
   * Check if long-press was triggered in this gesture.
   */
  bool wasTriggered() const {
    return triggered;
  }

  // ==========================================================================
  // Configuration

  /**
   * Set threshold duration in milliseconds.
   */
  void setThreshold(int thresholdMs) {
    threshold = thresholdMs;
  }

  /**
   * Get threshold duration.
   */
  int getThreshold() const {
    return threshold;
  }

  // ==========================================================================
  // Callbacks

  /**
   * Called when long-press threshold is reached.
   */
  std::function<void()> onLongPress;

private:
  void timerCallback() override {
    stopTimer();
    triggered = true;

    if (onLongPress)
      onLongPress();
  }

  int threshold = 500;
  bool triggered = false;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LongPressDetector)
};

// ==============================================================================
/**
 * LongPressWithProgress - Long-press with visual progress feedback
 *
 * Provides progress updates during the long-press hold, useful for
 * showing a radial progress indicator or fill animation.
 *
 * Usage:
 *   LongPressWithProgress longPress(500);  // 500ms
 *   longPress.onProgress = [this](float p) { progress = p; repaint(); };
 *   longPress.onLongPress = [this]() { triggerAction(); };
 */
class LongPressWithProgress : private juce::Timer {
public:
  explicit LongPressWithProgress(int thresholdMs = 500, int updateIntervalMs = 16)
      : threshold(thresholdMs), updateInterval(updateIntervalMs) {}

  ~LongPressWithProgress() override {
    stopTimer();
  }

  // ==========================================================================
  // Control

  void start() {
    triggered = false;
    startTimeMs = juce::Time::currentTimeMillis();
    startTimer(updateInterval);
  }

  void cancel() {
    stopTimer();
    if (onProgress)
      onProgress(0.0f);
  }

  void reset() {
    cancel();
    triggered = false;
  }

  // ==========================================================================
  // State

  bool isActive() const {
    return isTimerRunning();
  }
  bool wasTriggered() const {
    return triggered;
  }
  float getProgress() const {
    return currentProgress;
  }

  // ==========================================================================
  // Configuration

  void setThreshold(int thresholdMs) {
    threshold = thresholdMs;
  }
  int getThreshold() const {
    return threshold;
  }

  // ==========================================================================
  // Callbacks

  std::function<void(float progress)> onProgress;
  std::function<void()> onLongPress;

private:
  void timerCallback() override {
    auto elapsedMs = juce::Time::currentTimeMillis() - startTimeMs;
    currentProgress = juce::jlimit(0.0f, 1.0f, static_cast<float>(elapsedMs) / static_cast<float>(threshold));

    if (onProgress)
      onProgress(currentProgress);

    if (currentProgress >= 1.0f) {
      stopTimer();
      triggered = true;

      if (onLongPress)
        onLongPress();
    }
  }

  int threshold = 500;
  int updateInterval = 16;
  juce::int64 startTimeMs = 0;
  float currentProgress = 0.0f;
  bool triggered = false;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LongPressWithProgress)
};
