#include "AnimationController.h"
#include <algorithm>
#include <chrono>

namespace Sidechain {
namespace UI {
namespace Animations {

std::unique_ptr<AnimationController> AnimationController::instance_;
std::recursive_mutex AnimationController::instanceMutex_;

AnimationController::AnimationController() : enabled_(true) {
  // Don't start timer yet - will start on first animation
}

AnimationController::~AnimationController() {
  cancelAll();
  stopTimer();
}

AnimationController &AnimationController::getInstance() {
  std::lock_guard<std::recursive_mutex> lock(instanceMutex_);
  if (!instance_) {
    instance_ = std::make_unique<AnimationController>();
  }
  return *instance_;
}

// ========== Animation Scheduling ==========

AnimationHandle AnimationController::schedule(std::shared_ptr<AnimationTimeline> timeline,
                                              juce::Component *component) {
  if (!enabled_ || !timeline) {
    return AnimationHandle();
  }

  uint64_t id = generateHandle();
  auto entry = std::make_shared<AnimationEntry>();
  entry->id = id;
  entry->timeline = timeline;
  entry->component = component;
  entry->animation = nullptr;

  animations_[id] = entry;

  if (!isTimerRunning()) {
    startTimer(16); // ~60fps @ 16ms
  }

  timeline->start();
  return AnimationHandle(id);
}

// ========== Fade Animations ==========

AnimationHandle AnimationController::fadeIn(juce::Component *component, int durationMs) {
  return fadeTo(component, 1.0f, durationMs);
}

AnimationHandle AnimationController::fadeOut(juce::Component *component, int durationMs) {
  return fadeTo(component, 0.0f, durationMs);
}

AnimationHandle AnimationController::fadeTo(juce::Component *component, float alpha,
                                             int durationMs) {
  if (!component) {
    return AnimationHandle();
  }

  float startAlpha = component->getAlpha();
  auto animation = createFadeAnimation(component, startAlpha, alpha, durationMs);
  return schedule(animation, component);
}

// ========== Slide Animations ==========

AnimationHandle AnimationController::slideInFromLeft(juce::Component *component,
                                                      int durationMs) {
  if (!component) {
    return AnimationHandle();
  }

  int startX = -component->getWidth();
  int startY = component->getY();
  int endX = component->getX();
  int endY = component->getY();

  auto timeline = createSlideAnimation(component, startX, startY, endX, endY, durationMs);
  return schedule(timeline, component);
}

AnimationHandle AnimationController::slideInFromRight(juce::Component *component,
                                                       int durationMs) {
  if (!component) {
    return AnimationHandle();
  }

  int startX = component->getParentWidth();
  int startY = component->getY();
  int endX = component->getX();
  int endY = component->getY();

  auto timeline = createSlideAnimation(component, startX, startY, endX, endY, durationMs);
  return schedule(timeline, component);
}

AnimationHandle AnimationController::slideInFromTop(juce::Component *component,
                                                     int durationMs) {
  if (!component) {
    return AnimationHandle();
  }

  int startX = component->getX();
  int startY = -component->getHeight();
  int endX = component->getX();
  int endY = component->getY();

  auto timeline = createSlideAnimation(component, startX, startY, endX, endY, durationMs);
  return schedule(timeline, component);
}

AnimationHandle AnimationController::slideInFromBottom(juce::Component *component,
                                                        int durationMs) {
  if (!component) {
    return AnimationHandle();
  }

  int startX = component->getX();
  int startY = component->getParentHeight();
  int endX = component->getX();
  int endY = component->getY();

  auto timeline = createSlideAnimation(component, startX, startY, endX, endY, durationMs);
  return schedule(timeline, component);
}

// ========== Scale Animations ==========

AnimationHandle AnimationController::scaleIn(juce::Component *component, int durationMs) {
  return scaleTo(component, 1.0f, durationMs);
}

AnimationHandle AnimationController::scaleOut(juce::Component *component, int durationMs) {
  return scaleTo(component, 0.0f, durationMs);
}

AnimationHandle AnimationController::scaleTo(juce::Component *component, float scale,
                                              int durationMs) {
  if (!component) {
    return AnimationHandle();
  }

  float startScale = 1.0f;
  auto animation = createScaleAnimation(component, startScale, scale, durationMs);
  return schedule(animation, component);
}

// ========== Animation Control ==========

void AnimationController::cancel(AnimationHandle handle) {
  if (!handle.isValid()) {
    return;
  }

  auto it = animations_.find(handle.getId());
  if (it != animations_.end()) {
    auto &entry = it->second;
    if (entry->isValid) {
      if (entry->animation && entry->animation->isRunning()) {
        entry->animation->cancel();
      }
      if (entry->timeline && entry->timeline->isRunning()) {
        entry->timeline->cancel();
      }
      if (entry->cancellationCallback) {
        entry->cancellationCallback();
      }
      entry->isValid = false;
    }
  }
}

void AnimationController::cancelAllForComponent(juce::Component *component) {
  std::vector<uint64_t> toRemove;

  for (auto &[id, entry] : animations_) {
    if (entry->component == component && entry->isValid) {
      if (entry->animation && entry->animation->isRunning()) {
        entry->animation->cancel();
      }
      if (entry->timeline && entry->timeline->isRunning()) {
        entry->timeline->cancel();
      }
      if (entry->cancellationCallback) {
        entry->cancellationCallback();
      }
      entry->isValid = false;
      toRemove.push_back(id);
    }
  }

  for (auto id : toRemove) {
    animations_.erase(id);
  }
}

void AnimationController::pause(AnimationHandle handle) {
  auto it = animations_.find(handle.getId());
  if (it != animations_.end()) {
    auto &entry = it->second;
    if (entry->isValid) {
      if (entry->animation && entry->animation->isRunning()) {
        entry->animation->pause();
      }
      if (entry->timeline && entry->timeline->isRunning()) {
        entry->timeline->pause();
      }
    }
  }
}

void AnimationController::resume(AnimationHandle handle) {
  auto it = animations_.find(handle.getId());
  if (it != animations_.end()) {
    auto &entry = it->second;
    if (entry->isValid) {
      if (entry->animation && entry->animation->isPaused()) {
        entry->animation->resume();
      }
      if (entry->timeline && entry->timeline->isRunning()) {
        entry->timeline->resume();
      }
    }
  }
}

void AnimationController::pauseAll() {
  for (auto &[id, entry] : animations_) {
    if (entry->isValid) {
      if (entry->animation && entry->animation->isRunning()) {
        entry->animation->pause();
      }
      if (entry->timeline && entry->timeline->isRunning()) {
        entry->timeline->pause();
      }
    }
  }
}

void AnimationController::resumeAll() {
  for (auto &[id, entry] : animations_) {
    if (entry->isValid) {
      if (entry->animation && entry->animation->isPaused()) {
        entry->animation->resume();
      }
      if (entry->timeline && entry->timeline->isRunning()) {
        entry->timeline->resume();
      }
    }
  }
}

void AnimationController::cancelAll() {
  std::vector<uint64_t> ids;
  for (auto &[id, entry] : animations_) {
    ids.push_back(id);
  }

  for (auto id : ids) {
    cancel(AnimationHandle(id));
  }

  animations_.clear();
  stopTimer();
}

// ========== Callbacks ==========

void AnimationController::onCompletion(AnimationHandle handle,
                                        std::function<void()> callback) {
  auto it = animations_.find(handle.getId());
  if (it != animations_.end()) {
    it->second->completionCallback = callback;
  }
}

void AnimationController::onCancellation(AnimationHandle handle,
                                          std::function<void()> callback) {
  auto it = animations_.find(handle.getId());
  if (it != animations_.end()) {
    it->second->cancellationCallback = callback;
  }
}

void AnimationController::onProgress(AnimationHandle handle,
                                      std::function<void(float)> callback) {
  auto it = animations_.find(handle.getId());
  if (it != animations_.end()) {
    it->second->progressCallback = callback;
  }
}

// ========== State Queries ==========

bool AnimationController::isRunning(AnimationHandle handle) const {
  auto it = animations_.find(handle.getId());
  if (it != animations_.end()) {
    const auto &entry = it->second;
    if (entry->animation) {
      return entry->animation->isRunning();
    }
    if (entry->timeline) {
      return entry->timeline->isRunning();
    }
  }
  return false;
}

size_t AnimationController::getActiveAnimationCount() const {
  size_t count = 0;
  for (const auto &[id, entry] : animations_) {
    if (entry->isValid) {
      ++count;
    }
  }
  return count;
}

size_t AnimationController::getAnimationCountForComponent(juce::Component *component) const {
  size_t count = 0;
  for (const auto &[id, entry] : animations_) {
    if (entry->component == component && entry->isValid) {
      ++count;
    }
  }
  return count;
}

bool AnimationController::hasAnimationsForComponent(juce::Component *component) const {
  return getAnimationCountForComponent(component) > 0;
}

float AnimationController::getProgress(AnimationHandle handle) const {
  auto it = animations_.find(handle.getId());
  if (it != animations_.end()) {
    const auto &entry = it->second;
    if (entry->animation) {
      return entry->animation->getProgress();
    }
    if (entry->timeline) {
      return entry->timeline->getProgress();
    }
  }
  return 0.0f;
}

// ========== Timer Callback ==========

void AnimationController::timerCallback() {
  updateAnimations();
  cleanupFinished();
}

void AnimationController::updateAnimations() {
  for (auto &[id, entry] : animations_) {
    if (!entry->isValid) {
      continue;
    }

    // Note: JUCE doesn't provide a safe way to check if a component is still valid
    // In production, components should manage their own animation lifecycle via destructor
    // For now, we proceed assuming components are still valid

    // Update animation progress callbacks
    if (entry->progressCallback) {
      float progress = 0.0f;
      if (entry->animation) {
        progress = entry->animation->getProgress();
      } else if (entry->timeline) {
        progress = entry->timeline->getProgress();
      }
      entry->progressCallback(progress);
    }

    // Check for completion
    bool isComplete = false;
    if (entry->animation) {
      isComplete = !entry->animation->isRunning();
    } else if (entry->timeline) {
      isComplete = !entry->timeline->isRunning();
    }

    if (isComplete && entry->isValid) {
      if (entry->completionCallback) {
        entry->completionCallback();
      }
      entry->isValid = false;
    }
  }
}

void AnimationController::cleanupFinished() {
  std::vector<uint64_t> toRemove;

  for (auto &[id, entry] : animations_) {
    if (!entry->isValid) {
      toRemove.push_back(id);
    }
  }

  for (auto id : toRemove) {
    animations_.erase(id);
  }

  // Stop timer if no more animations
  if (animations_.empty() && isTimerRunning()) {
    stopTimer();
  }
}

// ========== Helpers ==========

uint64_t AnimationController::generateHandle() {
  return nextHandleId_.fetch_add(1, std::memory_order_relaxed);
}

std::shared_ptr<TransitionAnimation<float>> AnimationController::createFadeAnimation(
    juce::Component *component, float startAlpha, float endAlpha, int durationMs) {
  auto animation = TransitionAnimation<float>::create(startAlpha, endAlpha, durationMs);

  animation->onProgress([component](float alpha) { component->setAlpha(alpha); });

  return animation;
}

std::shared_ptr<AnimationTimeline> AnimationController::createSlideAnimation(
    juce::Component *component, int startX, int startY, int endX, int endY,
    int durationMs) {
  auto timeline = AnimationTimeline::parallel();

  auto xAnim = TransitionAnimation<int>::create(startX, endX, durationMs);
  xAnim->onProgress([component](int x) {
    auto bounds = component->getBounds();
    component->setBounds(x, bounds.getY(), bounds.getWidth(), bounds.getHeight());
  });

  auto yAnim = TransitionAnimation<int>::create(startY, endY, durationMs);
  yAnim->onProgress([component](int y) {
    auto bounds = component->getBounds();
    component->setBounds(bounds.getX(), y, bounds.getWidth(), bounds.getHeight());
  });

  timeline->add(xAnim, durationMs)->add(yAnim, durationMs);

  return timeline;
}

std::shared_ptr<TransitionAnimation<float>> AnimationController::createScaleAnimation(
    juce::Component *component, float startScale, float endScale, int durationMs) {
  auto animation = TransitionAnimation<float>::create(startScale, endScale, durationMs);

  int originalWidth = component->getWidth();
  int originalHeight = component->getHeight();
  auto originalBounds = component->getBounds();

  animation->onProgress([component, originalBounds, originalWidth, originalHeight](
                            float scale) {
    int newWidth = static_cast<int>(originalWidth * scale);
    int newHeight = static_cast<int>(originalHeight * scale);
    int newX = originalBounds.getX() + (originalWidth - newWidth) / 2;
    int newY = originalBounds.getY() + (originalHeight - newHeight) / 2;

    component->setBounds(newX, newY, newWidth, newHeight);
  });

  return animation;
}

} // namespace Animations
} // namespace UI
} // namespace Sidechain
