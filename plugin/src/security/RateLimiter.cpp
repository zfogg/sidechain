#include "RateLimiter.h"

namespace Sidechain {
namespace Security {

// ========== TokenBucketLimiter ==========

TokenBucketLimiter::TokenBucketLimiter(const RateLimitConfig &config)
    : config_(config), lastCleanup_(std::chrono::steady_clock::now()) {}

double TokenBucketLimiter::getRefillRate() const {
  return static_cast<double>(config_.rateLimit) / config_.windowSeconds;
}

TokenBucketLimiter::BucketState &TokenBucketLimiter::getOrCreateBucket(const juce::String &identifier) {
  if (!buckets_.contains(identifier)) {
    buckets_.set(identifier, BucketState{static_cast<double>(config_.burstSize), std::chrono::steady_clock::now()});
  }
  return buckets_.getReference(identifier);
}

void TokenBucketLimiter::refillBucket(BucketState &bucket) {
  auto now = std::chrono::steady_clock::now();
  auto elapsedSeconds = std::chrono::duration<double>(now - bucket.lastRefillTime).count();

  double tokensToAdd = elapsedSeconds * getRefillRate();
  bucket.tokens = std::min(bucket.tokens + tokensToAdd, static_cast<double>(config_.rateLimit));
  bucket.lastRefillTime = now;
}

RateLimitStatus TokenBucketLimiter::tryConsume(const juce::String &identifier, int tokens) {
  std::lock_guard<std::mutex> lock(mutex_);

  auto &bucket = getOrCreateBucket(identifier);
  refillBucket(bucket);

  RateLimitStatus status;
  status.limit = config_.rateLimit;
  status.resetInSeconds = config_.windowSeconds;

  if (bucket.tokens >= tokens) {
    bucket.tokens -= tokens;
    status.allowed = true;
    status.remaining = static_cast<int>(bucket.tokens);
  } else {
    status.allowed = false;
    status.remaining = 0;
    status.retryAfterSeconds = static_cast<int>((tokens - bucket.tokens) / getRefillRate());
  }

  // Periodic cleanup
  auto now = std::chrono::steady_clock::now();
  if (std::chrono::duration_cast<std::chrono::minutes>(now - lastCleanup_).count() >= config_.cleanupIntervalMinutes) {
    cleanup();
  }

  return status;
}

void TokenBucketLimiter::reset(const juce::String &identifier) {
  std::lock_guard<std::mutex> lock(mutex_);
  buckets_.remove(identifier);
}

RateLimitStatus TokenBucketLimiter::getStatus(const juce::String &identifier) const {
  std::lock_guard<std::mutex> lock(mutex_);

  if (!buckets_.contains(identifier)) {
    return RateLimitStatus{true, config_.rateLimit, config_.rateLimit, config_.windowSeconds, -1};
  }

  auto bucket = buckets_[identifier];
  auto now = std::chrono::steady_clock::now();
  auto elapsedSeconds = std::chrono::duration<double>(now - bucket.lastRefillTime).count();
  double tokensNow =
      std::min(bucket.tokens + (elapsedSeconds * getRefillRate()), static_cast<double>(config_.rateLimit));

  return RateLimitStatus{true, static_cast<int>(tokensNow), config_.rateLimit, config_.windowSeconds, -1};
}

size_t TokenBucketLimiter::getTrackedCount() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return static_cast<size_t>(buckets_.size());
}

void TokenBucketLimiter::cleanup() {
  // Remove entries with low token counts (idle)
  auto now = std::chrono::steady_clock::now();
  juce::Array<juce::String> keysToRemove;

  for (auto it = buckets_.begin(); it != buckets_.end(); ++it) {
    auto elapsedMinutes = std::chrono::duration_cast<std::chrono::minutes>(now - it.getValue().lastRefillTime).count();

    if (elapsedMinutes > config_.cleanupIntervalMinutes) {
      keysToRemove.add(it.getKey());
    }

    // Stop if we're under max size
    if (static_cast<size_t>(buckets_.size()) < config_.maxTrackedIdentifiers)
      break;
  }

  for (const auto &key : keysToRemove)
    buckets_.remove(key);

  lastCleanup_ = now;
}

// ========== SlidingWindowLimiter ==========

SlidingWindowLimiter::SlidingWindowLimiter(const RateLimitConfig &config)
    : config_(config), lastGlobalCleanup_(std::chrono::steady_clock::now()) {}

int SlidingWindowLimiter::getRequestsInWindow(const WindowState &window) const {
  auto now = std::chrono::steady_clock::now();
  auto windowStart = now - std::chrono::seconds(config_.windowSeconds);

  int count = 0;
  for (const auto &requestTime : window.requests) {
    if (requestTime > windowStart)
      count++;
  }
  return count;
}

void SlidingWindowLimiter::cleanupWindow(WindowState &window) {
  auto now = std::chrono::steady_clock::now();
  auto windowStart = now - std::chrono::seconds(config_.windowSeconds);

  auto it = window.requests.begin();
  while (it != window.requests.end()) {
    if (*it <= windowStart)
      it = window.requests.erase(it);
    else
      ++it;
  }
}

RateLimitStatus SlidingWindowLimiter::tryConsume(const juce::String &identifier, int tokens) {
  std::lock_guard<std::mutex> lock(mutex_);

  if (!windows_.contains(identifier)) {
    windows_.set(identifier,
                 WindowState{std::vector<std::chrono::steady_clock::time_point>(), std::chrono::steady_clock::now()});
  }

  auto &window = windows_.getReference(identifier);
  cleanupWindow(window);

  RateLimitStatus status;
  status.limit = config_.rateLimit;
  status.resetInSeconds = config_.windowSeconds;

  int currentRequests = getRequestsInWindow(window);
  if (currentRequests + tokens <= config_.rateLimit) {
    for (int i = 0; i < tokens; ++i) {
      window.requests.push_back(std::chrono::steady_clock::now());
    }
    status.allowed = true;
    status.remaining = config_.rateLimit - (currentRequests + tokens);
  } else {
    status.allowed = false;
    status.remaining = std::max(0, config_.rateLimit - currentRequests);

    // Calculate retry time
    if (!window.requests.empty()) {
      auto oldestRequest = window.requests.front();
      auto windowStart = std::chrono::steady_clock::now() - std::chrono::seconds(config_.windowSeconds);
      status.retryAfterSeconds = std::max(
          1, static_cast<int>(std::chrono::duration_cast<std::chrono::seconds>(oldestRequest - windowStart).count()));
    }
  }

  // Periodic cleanup
  auto now = std::chrono::steady_clock::now();
  if (std::chrono::duration_cast<std::chrono::minutes>(now - lastGlobalCleanup_).count() >=
      config_.cleanupIntervalMinutes) {
    // Clean up old windows
    juce::Array<juce::String> emptyWindows;
    for (auto it = windows_.begin(); it != windows_.end(); ++it) {
      auto &windowState = windows_.getReference(it.getKey());
      cleanupWindow(windowState);
      if (windowState.requests.empty())
        emptyWindows.add(it.getKey());
    }

    for (const auto &key : emptyWindows)
      windows_.remove(key);

    lastGlobalCleanup_ = now;
  }

  return status;
}

void SlidingWindowLimiter::reset(const juce::String &identifier) {
  std::lock_guard<std::mutex> lock(mutex_);
  windows_.remove(identifier);
}

RateLimitStatus SlidingWindowLimiter::getStatus(const juce::String &identifier) const {
  std::lock_guard<std::mutex> lock(mutex_);

  if (!windows_.contains(identifier)) {
    return RateLimitStatus{true, config_.rateLimit, config_.rateLimit, config_.windowSeconds, -1};
  }

  int currentRequests = getRequestsInWindow(windows_[identifier]);
  return RateLimitStatus{true, config_.rateLimit - currentRequests, config_.rateLimit, config_.windowSeconds, -1};
}

size_t SlidingWindowLimiter::getTrackedCount() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return static_cast<size_t>(windows_.size());
}

void SlidingWindowLimiter::cleanup() {
  std::lock_guard<std::mutex> lock(mutex_);

  auto now = std::chrono::steady_clock::now();
  juce::Array<juce::String> keysToRemove;

  for (auto it = windows_.begin(); it != windows_.end(); ++it) {
    auto elapsedMinutes = std::chrono::duration_cast<std::chrono::minutes>(now - it.getValue().lastCleanup).count();

    if (elapsedMinutes > config_.cleanupIntervalMinutes) {
      keysToRemove.add(it.getKey());
    }

    if (static_cast<size_t>(windows_.size()) < config_.maxTrackedIdentifiers)
      break;
  }

  for (const auto &key : keysToRemove)
    windows_.remove(key);
}

// ========== RateLimiter ==========

RateLimiter::RateLimiter() : std::enable_shared_from_this<RateLimiter>() {
  initializeImplementation();
}

void RateLimiter::initializeImplementation() {
  if (algorithm_ == Algorithm::TokenBucket) {
    tokenBucketImpl_ = std::make_unique<TokenBucketLimiter>(config_);
    slidingWindowImpl_ = nullptr;
  } else {
    slidingWindowImpl_ = std::make_unique<SlidingWindowLimiter>(config_);
    tokenBucketImpl_ = nullptr;
  }
}

RateLimitStatus RateLimiter::tryConsume(const juce::String &identifier, int tokens) {
  if (algorithm_ == Algorithm::TokenBucket && tokenBucketImpl_)
    return tokenBucketImpl_->tryConsume(identifier, tokens);
  else if (algorithm_ == Algorithm::SlidingWindow && slidingWindowImpl_)
    return slidingWindowImpl_->tryConsume(identifier, tokens);

  return RateLimitStatus{};
}

RateLimitStatus RateLimiter::getStatus(const juce::String &identifier) const {
  if (algorithm_ == Algorithm::TokenBucket && tokenBucketImpl_)
    return tokenBucketImpl_->getStatus(identifier);
  else if (algorithm_ == Algorithm::SlidingWindow && slidingWindowImpl_)
    return slidingWindowImpl_->getStatus(identifier);

  return RateLimitStatus{};
}

void RateLimiter::reset(const juce::String &identifier) {
  if (algorithm_ == Algorithm::TokenBucket && tokenBucketImpl_)
    tokenBucketImpl_->reset(identifier);
  else if (algorithm_ == Algorithm::SlidingWindow && slidingWindowImpl_)
    slidingWindowImpl_->reset(identifier);
}

void RateLimiter::resetAll() {
  initializeImplementation();
}

size_t RateLimiter::getTrackedCount() const {
  if (algorithm_ == Algorithm::TokenBucket && tokenBucketImpl_)
    return tokenBucketImpl_->getTrackedCount();
  else if (algorithm_ == Algorithm::SlidingWindow && slidingWindowImpl_)
    return slidingWindowImpl_->getTrackedCount();

  return 0;
}

void RateLimiter::cleanup() {
  if (algorithm_ == Algorithm::TokenBucket && tokenBucketImpl_)
    tokenBucketImpl_->cleanup();
  else if (algorithm_ == Algorithm::SlidingWindow && slidingWindowImpl_)
    slidingWindowImpl_->cleanup();
}

} // namespace Security
} // namespace Sidechain
