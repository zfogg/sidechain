#pragma once

#include <JuceHeader.h>
#include <chrono>
#include <unordered_map>
#include <mutex>
#include <memory>
#include <optional>

namespace Sidechain {
namespace Security {

/**
 * RateLimiter - Token bucket based rate limiting
 *
 * Prevents abuse by limiting request rate per identifier (user, IP, API key, etc.)
 *
 * Algorithms supported:
 * - Token Bucket: Fixed rate with burst allowance
 * - Sliding Window: Time-based request counting
 * - Leaky Bucket: Smooth request processing
 *
 * Usage:
 *   auto limiter = RateLimiter::create()
 *       ->setRate(100)           // 100 requests
 *       ->setWindow(60)           // per 60 seconds
 *       ->setBurstSize(20);       // burst up to 20
 *
 *   if (limiter->tryConsume("user_id_123", 1))
 *       processRequest();
 *   else
 *       rejectRequest("Rate limit exceeded");
 */

struct RateLimitConfig
{
    /**
     * Number of requests allowed in the window
     */
    int rateLimit = 100;

    /**
     * Time window in seconds
     */
    int windowSeconds = 60;

    /**
     * Burst size (tokens available immediately)
     */
    int burstSize = 20;

    /**
     * Clean up old entries after this many minutes of inactivity
     */
    int cleanupIntervalMinutes = 60;

    /**
     * Maximum number of unique identifiers to track
     */
    int maxTrackedIdentifiers = 10000;
};

/**
 * RateLimitStatus - Result of rate limit check
 */
struct RateLimitStatus
{
    /**
     * Whether the request is allowed
     */
    bool allowed = false;

    /**
     * Remaining requests in current window
     */
    int remaining = 0;

    /**
     * Total limit for the window
     */
    int limit = 0;

    /**
     * Seconds until limit resets
     */
    int resetInSeconds = 0;

    /**
     * Retry after this many seconds (if rate limited)
     */
    int retryAfterSeconds = -1;
};

/**
 * TokenBucketLimiter - Token bucket algorithm implementation
 *
 * Allows burst traffic up to burstSize tokens, then refills at a constant rate
 */
class TokenBucketLimiter
{
public:
    explicit TokenBucketLimiter(const RateLimitConfig& config);

    /**
     * Try to consume tokens for an identifier
     *
     * @param identifier Unique identifier (user ID, IP, etc.)
     * @param tokens Number of tokens to consume
     * @return Status including whether request is allowed
     */
    RateLimitStatus tryConsume(const juce::String& identifier, int tokens = 1);

    /**
     * Reset rate limit for an identifier
     */
    void reset(const juce::String& identifier);

    /**
     * Get current status without consuming tokens
     */
    RateLimitStatus getStatus(const juce::String& identifier) const;

    /**
     * Get number of tracked identifiers
     */
    size_t getTrackedCount() const;

    /**
     * Clean up inactive entries
     */
    void cleanup();

private:
    struct BucketState
    {
        double tokens;
        std::chrono::steady_clock::time_point lastRefillTime;
    };

    RateLimitConfig config_;
    mutable std::mutex mutex_;
    std::unordered_map<std::string, BucketState> buckets_;
    std::chrono::steady_clock::time_point lastCleanup_;

    double getRefillRate() const;
    BucketState& getOrCreateBucket(const juce::String& identifier);
    void refillBucket(BucketState& bucket);
};

/**
 * SlidingWindowLimiter - Sliding window counter algorithm
 *
 * Tracks actual request times and enforces limit over a moving window
 */
class SlidingWindowLimiter
{
public:
    explicit SlidingWindowLimiter(const RateLimitConfig& config);

    /**
     * Try to consume for an identifier
     */
    RateLimitStatus tryConsume(const juce::String& identifier, int tokens = 1);

    /**
     * Reset rate limit for an identifier
     */
    void reset(const juce::String& identifier);

    /**
     * Get current status
     */
    RateLimitStatus getStatus(const juce::String& identifier) const;

    /**
     * Get number of tracked identifiers
     */
    size_t getTrackedCount() const;

    /**
     * Clean up old entries
     */
    void cleanup();

private:
    struct WindowState
    {
        std::vector<std::chrono::steady_clock::time_point> requests;
        std::chrono::steady_clock::time_point lastCleanup;
    };

    RateLimitConfig config_;
    mutable std::mutex mutex_;
    std::unordered_map<std::string, WindowState> windows_;
    std::chrono::steady_clock::time_point lastGlobalCleanup_;

    int getRequestsInWindow(const WindowState& window) const;
    void cleanupWindow(WindowState& window);
};

/**
 * RateLimiter - Factory and main interface
 */
class RateLimiter : public std::enable_shared_from_this<RateLimiter>
{
public:
    enum class Algorithm
    {
        TokenBucket,
        SlidingWindow,
    };

    /**
     * Create a new rate limiter
     */
    static std::shared_ptr<RateLimiter> create()
    {
        return std::make_shared<RateLimiter>();
    }

    /**
     * Set the rate (number of requests)
     */
    std::shared_ptr<RateLimiter> setRate(int rate)
    {
        config_.rateLimit = rate;
        return shared_from_this();
    }

    /**
     * Set the time window (in seconds)
     */
    std::shared_ptr<RateLimiter> setWindow(int seconds)
    {
        config_.windowSeconds = seconds;
        return shared_from_this();
    }

    /**
     * Set burst size (tokens available immediately)
     */
    std::shared_ptr<RateLimiter> setBurstSize(int size)
    {
        config_.burstSize = size;
        return shared_from_this();
    }

    /**
     * Set cleanup interval
     */
    std::shared_ptr<RateLimiter> setCleanupInterval(int minutes)
    {
        config_.cleanupIntervalMinutes = minutes;
        return shared_from_this();
    }

    /**
     * Set max tracked identifiers
     */
    std::shared_ptr<RateLimiter> setMaxTrackedIdentifiers(int count)
    {
        config_.maxTrackedIdentifiers = count;
        return shared_from_this();
    }

    /**
     * Set algorithm (Token Bucket is default)
     */
    std::shared_ptr<RateLimiter> setAlgorithm(Algorithm algo)
    {
        algorithm_ = algo;
        initializeImplementation();
        return shared_from_this();
    }

    /**
     * Check if request is allowed and consume tokens
     *
     * @param identifier Unique request identifier
     * @param tokens Number of tokens to consume (default 1)
     * @return Status with allow/deny and remaining count
     */
    RateLimitStatus tryConsume(const juce::String& identifier, int tokens = 1);

    /**
     * Check status without consuming tokens
     */
    RateLimitStatus getStatus(const juce::String& identifier) const;

    /**
     * Reset rate limit for an identifier
     */
    void reset(const juce::String& identifier);

    /**
     * Reset all rate limits
     */
    void resetAll();

    /**
     * Get number of tracked identifiers
     */
    size_t getTrackedCount() const;

    /**
     * Clean up inactive entries
     */
    void cleanup();

    /**
     * Get configuration
     */
    const RateLimitConfig& getConfig() const { return config_; }

    /**
     * Constructor (use create() instead for proper shared_ptr management)
     */
    RateLimiter();

private:
    void initializeImplementation();

    RateLimitConfig config_;
    Algorithm algorithm_ = Algorithm::TokenBucket;

    std::unique_ptr<TokenBucketLimiter> tokenBucketImpl_;
    std::unique_ptr<SlidingWindowLimiter> slidingWindowImpl_;
};

/**
 * RateLimitMiddleware - JUCE integration for automatic rate limiting
 *
 * Can be used with JUCE HTTP servers to automatically rate limit incoming requests
 */
class RateLimitMiddleware
{
public:
    /**
     * Create middleware for rate limiting
     *
     * @param limiter The rate limiter instance
     * @param identifierExtractor Function to extract identifier from context
     */
    RateLimitMiddleware(std::shared_ptr<RateLimiter> limiter,
                        std::function<juce::String(const juce::String&)> identifierExtractor)
        : limiter_(limiter), identifierExtractor_(identifierExtractor)
    {
    }

    /**
     * Check if request should be allowed
     *
     * @param context Request context (e.g., remote address, user ID)
     * @return Status of the rate limit check
     */
    RateLimitStatus checkRequest(const juce::String& context)
    {
        auto identifier = identifierExtractor_(context);
        return limiter_->tryConsume(identifier);
    }

private:
    std::shared_ptr<RateLimiter> limiter_;
    std::function<juce::String(const juce::String&)> identifierExtractor_;
};

}  // namespace Security
}  // namespace Sidechain
