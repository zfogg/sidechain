#include "NavigationStack.h"
#include <memory>

namespace Sidechain {
namespace UI {
namespace Animations {

NavigationStack::NavigationStack(juce::Component *parent, size_t initialCapacity)
    : parent_(parent), defaultPushTransition_(TransitionType::SlideInFromRight),
      defaultPopTransition_(TransitionType::SlideOutToRight), defaultPushDurationMs_(300),
      defaultPopDurationMs_(300), transitionsEnabled_(true), activeTransitionCount_(0) {
  // Note: std::deque doesn't support reserve(), allocation is dynamic

  // Add this component to parent
  if (parent_) {
    parent_->addAndMakeVisible(this);
  }
}

NavigationStack::~NavigationStack() {
  clear();
}

void NavigationStack::push(std::unique_ptr<juce::Component> view, TransitionType transition,
                           int durationMs, NavigationCallback onComplete) {
  if (!view) {
    return;
  }

  // Get previous view before adding new one
  juce::Component *previousView = getCurrentView();

  // Hide previous view with exit animation
  if (previousView && transitionsEnabled_) {
    applyExitTransition(previousView, TransitionType::FadeOut, durationMs / 2, nullptr);
  } else if (previousView) {
    hideView(previousView);
  }

  // Add new view to stack
  stack_.emplace_back(std::move(view));
  juce::Component *newView = getCurrentView();

  // Add as child and apply entry animation
  if (newView) {
    addAndMakeVisible(newView);
    newView->setBounds(getBounds());

    if (transitionsEnabled_) {
      applyEntryTransition(
          newView, transition, durationMs, [this, onComplete, newView, previousView](juce::Component *nv, juce::Component *pv) {
            if (onComplete) {
              onComplete(newView, previousView);
            }
            if (navigationCallback_) {
              navigationCallback_(newView, previousView);
            }
          });
    } else {
      if (onComplete) {
        onComplete(newView, previousView);
      }
      if (navigationCallback_) {
        navigationCallback_(newView, previousView);
      }
    }
  }
}

std::unique_ptr<juce::Component> NavigationStack::pop(TransitionType transition, int durationMs,
                                                       NavigationCallback onComplete) {
  if (stack_.empty()) {
    return nullptr;
  }

  juce::Component *currentView = getCurrentView();
  juce::Component *nextView = nullptr;

  // Apply exit transition to current view
  if (currentView && transitionsEnabled_) {
    applyExitTransition(currentView, transition, durationMs, nullptr);
  }

  // Remove current view from stack
  auto popped = std::move(stack_.back().view);
  stack_.pop_back();

  // Show previous view with entry animation
  if (!stack_.empty()) {
    nextView = getCurrentView();
    if (nextView) {
      showView(nextView);

      if (transitionsEnabled_) {
        applyEntryTransition(
            nextView, TransitionType::FadeIn, durationMs / 2, [this, onComplete, nextView, currentView](juce::Component *nv, juce::Component *pv) {
              if (onComplete) {
                onComplete(nextView, currentView);
              }
              if (navigationCallback_) {
                navigationCallback_(nextView, currentView);
              }
            });
      } else {
        if (onComplete) {
          onComplete(nextView, currentView);
        }
        if (navigationCallback_) {
          navigationCallback_(nextView, currentView);
        }
      }
    }
  } else {
    if (onComplete) {
      onComplete(nullptr, currentView);
    }
    if (navigationCallback_) {
      navigationCallback_(nullptr, currentView);
    }
  }

  return popped;
}

void NavigationStack::replace(std::unique_ptr<juce::Component> view, TransitionType transition,
                              int durationMs, NavigationCallback onComplete) {
  if (!view) {
    return;
  }

  // Capture view in a shared_ptr to ensure it stays alive until push completes
  auto viewPtr = std::make_shared<std::unique_ptr<juce::Component>>(std::move(view));

  pop(TransitionType::FadeOut, durationMs / 2, [this, viewPtr, transition, durationMs, onComplete](
                                                  juce::Component *newView, juce::Component *prevView) {
    // Push new view after pop completes
    if (viewPtr && *viewPtr) {
      push(std::move(*viewPtr), transition, durationMs, onComplete);
    }
  });
}

void NavigationStack::popToRoot(TransitionType transition, int durationMs) {
  while (stack_.size() > 1) {
    pop(transition, durationMs);
  }
}

void NavigationStack::clear() {
  while (!stack_.empty()) {
    auto view = std::move(stack_.back().view);
    stack_.pop_back();
    // view is automatically destroyed here
  }
}

juce::Component *NavigationStack::getCurrentView() const {
  if (stack_.empty()) {
    return nullptr;
  }
  return stack_.back().view.get();
}

juce::Component *NavigationStack::getPreviousView() const {
  if (stack_.size() < 2) {
    return nullptr;
  }
  return stack_[stack_.size() - 2].view.get();
}

size_t NavigationStack::getStackDepth() const {
  return stack_.size();
}

bool NavigationStack::isEmpty() const {
  return stack_.empty();
}

bool NavigationStack::isAnimating() const {
  return activeTransitionCount_ > 0;
}

void NavigationStack::setDefaultPushTransition(TransitionType type, int durationMs) {
  defaultPushTransition_ = type;
  defaultPushDurationMs_ = durationMs;
}

void NavigationStack::setDefaultPopTransition(TransitionType type, int durationMs) {
  defaultPopTransition_ = type;
  defaultPopDurationMs_ = durationMs;
}

void NavigationStack::setNavigationCallback(NavigationCallback callback) {
  navigationCallback_ = callback;
}

void NavigationStack::setTransitionsEnabled(bool enabled) {
  transitionsEnabled_ = enabled;
}

void NavigationStack::resized() {
  // Update all views in the stack to fill the container
  for (auto &entry : stack_) {
    if (entry.view) {
      entry.view->setBounds(getBounds());
    }
  }
}

void NavigationStack::applyEntryTransition(juce::Component *view, TransitionType type,
                                            int durationMs, NavigationCallback onComplete) {
  if (!view) {
    return;
  }

  auto &controller = AnimationController::getInstance();
  AnimationHandle handle;

  switch (type) {
    case TransitionType::SlideInFromLeft:
      handle = controller.slideInFromLeft(view, durationMs);
      break;
    case TransitionType::SlideInFromRight:
      handle = controller.slideInFromRight(view, durationMs);
      break;
    case TransitionType::SlideInFromTop:
      handle = controller.slideInFromTop(view, durationMs);
      break;
    case TransitionType::SlideInFromBottom:
      handle = controller.slideInFromBottom(view, durationMs);
      break;
    case TransitionType::FadeIn:
      handle = controller.fadeIn(view, durationMs);
      break;
    case TransitionType::ScaleIn:
      handle = controller.scaleIn(view, durationMs);
      break;
    case TransitionType::ZoomIn:
      handle = controller.scaleIn(view, durationMs);
      break;
    case TransitionType::CrossFade:
      // CrossFade is fade in of new view + fade out of previous (handled elsewhere)
      handle = controller.fadeIn(view, durationMs);
      break;
    case TransitionType::Instant:
      // No animation
      if (onComplete) {
        onComplete(view, nullptr);
      }
      return;
    default:
      handle = controller.fadeIn(view, durationMs);
      break;
  }

  if (onComplete) {
    // Note: AnimationController callbacks don't include component parameters
    // We wrap to call onComplete with the correct signature
    controller.onCompletion(handle, [onComplete, view]() {
      onComplete(view, nullptr);
    });
  }
}

void NavigationStack::applyExitTransition(juce::Component *view, TransitionType type,
                                           int durationMs, NavigationCallback onComplete) {
  if (!view) {
    return;
  }

  auto &controller = AnimationController::getInstance();
  AnimationHandle handle;

  switch (type) {
    case TransitionType::SlideOutToLeft:
      // For slide out, we'd need controller to support slide out animations
      // For now, fall through to fade
      [[fallthrough]];
    case TransitionType::SlideOutToRight:
    case TransitionType::SlideOutToTop:
    case TransitionType::SlideOutToBottom:
      // Slide out animations not yet implemented, use fade
      handle = controller.fadeOut(view, durationMs);
      break;
    case TransitionType::FadeOut:
      handle = controller.fadeOut(view, durationMs);
      break;
    case TransitionType::ScaleOut:
      handle = controller.scaleOut(view, durationMs);
      break;
    case TransitionType::ZoomOut:
      handle = controller.scaleOut(view, durationMs);
      break;
    case TransitionType::CrossFade:
      handle = controller.fadeOut(view, durationMs);
      break;
    case TransitionType::Instant:
      hideView(view);
      if (onComplete) {
        onComplete(view, nullptr);
      }
      return;
    default:
      handle = controller.fadeOut(view, durationMs);
      break;
  }

  if (onComplete) {
    controller.onCompletion(handle, [onComplete, view, this]() {
      hideView(view);
      onComplete(view, nullptr);
    });
  } else {
    controller.onCompletion(
        handle, [view, this]() { hideView(view); });
  }
}

void NavigationStack::showView(juce::Component *view) {
  if (view) {
    view->setVisible(true);
    view->setAlpha(1.0f);
    toFront(false);
  }
}

void NavigationStack::hideView(juce::Component *view) {
  if (view) {
    view->setVisible(false);
    view->setAlpha(0.0f);
  }
}

void NavigationStack::onTransitionComplete() {
  // Called when a transition animation completes
  activeTransitionCount_--;
}

} // namespace Animations
} // namespace UI
} // namespace Sidechain
